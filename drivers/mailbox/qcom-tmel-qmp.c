// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <dm.h>
#include <malloc.h>
#include <mailbox-uclass.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/tmelcom-qmp.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <asm/gic.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>

DECLARE_GLOBAL_DATA_PTR;

#define QMP_NUM_CHANS		0x1
#define QMP_TOUT_MS		1000
#define QMP_CTRL_DATA_SIZE	4
#define QMP_MAX_PKT_SIZE	0x18
#define QMP_UCORE_DESC_OFFSET	0x1000
#define QMP_SEND_TIMEOUT	30000

#define QMP_HW_MBOX_SIZE		32
#define QMP_MBOX_RSV_SIZE		4
#define QMP_MBOX_IPC_PACKET_SIZE	(QMP_HW_MBOX_SIZE - QMP_CTRL_DATA_SIZE - QMP_MBOX_RSV_SIZE)
#define QMP_MBOX_IPC_MAX_PARAMS		5

#define QMP_MAX_PARAM_IN_PARAM_ID	14
#define QMP_PARAM_CNT_FOR_OUTBUF	3
#define QMP_SRAM_IPC_MAX_PARAMS		(QMP_MAX_PARAM_IN_PARAM_ID * QMP_PARAM_CNT_FOR_OUTBUF)
#define QMP_SRAM_IPC_MAX_BUF_SIZE	(QMP_SRAM_IPC_MAX_PARAMS * sizeof(u32))

#define TMEL_ERROR_GENERIC		(0x1u)
#define TMEL_ERROR_NOT_SUPPORTED	(0x2u)
#define TMEL_ERROR_BAD_PARAMETER	(0x3u)
#define TMEL_ERROR_BAD_MESSAGE		(0x4u)
#define TMEL_ERROR_BAD_ADDRESS		(0x5u)
#define TMEL_ERROR_TMELCOM_FAILURE	(0x6u)
#define TMEL_ERROR_TMEL_BUSY		(0x7u)

/*
 * mbox data can be shared over mem or sram
 */
enum ipc_type {
	IPC_MBOX_MEM,
	IPC_MBOX_SRAM,
};

/*
 * mbox header indicates the type of payload and action required.
 */
struct ipc_header {
	u8 ipc_type:1;
	u8 msg_len:7;
	u8 msg_type;
	u8 action_id;
	s8 response;
};

struct mbox_payload {
	u32 param[QMP_MBOX_IPC_MAX_PARAMS];
};

struct sram_payload {
	u32 payload_ptr;
	u32 payload_len;
};

union ipc_payload {
	struct mbox_payload mbox_payload;
	struct sram_payload sram_payload;
};

struct tmel_ipc_pkt {
	struct ipc_header msg_hdr;
	union ipc_payload payload;
};

/**
 * enum qmp_local_state - definition of the local state machine
 * @LINK_DISCONNECTED: Init state, waiting for ucore to start
 * @LINK_NEGOTIATION: Set local link state to up, wait for ucore ack
 * @LINK_CONNECTED: Link state up, channel not connected
 * @LOCAL_CONNECTING: Channel opening locally, wait for ucore ack
 * @CHANNEL_CONNECTED: Channel fully opened
 * @LOCAL_DISCONNECTING: Channel disconnected locally, wait for ucore ack
 */
enum qmp_local_state {
	LINK_DISCONNECTED,
	LINK_NEGOTIATION,
	LINK_CONNECTED,
	LOCAL_CONNECTING,
	CHANNEL_CONNECTED,
	LOCAL_DISCONNECTING,
};

/**
 * struct qmp_channel_desc - IPC bits
 * @bits: Var to access each member
 * @val: u32 representation of above
 */
union qmp_channel_desc {
	struct {
		u32 link_state:1;
		u32 link_state_ack:1;
		u32 ch_state:1;
		u32 ch_state_ack:1;
		u32 tx:1;
		u32 tx_ack:1;
		u32 rx_done:1;
		u32 rx_done_ack:1;
		u32 reserved:8;
		u32 frag_size:8;
		u32 rem_frag_count:8;
	} bits;
	unsigned int val;
};

struct iovec_tmel {
	void *iov_base;	/* Pointer to data.  */
	size_t iov_len;	/* Length of data.  */
};

/**
 * struct qmp_device - local information for managing a single mailbox
 * @dev: The device that corresponds to this mailbox
 * @mcore_desc: Local core (APSS) mailbox descriptor
 * @ucore_desc: Remote core (TME-L) mailbox descriptor
 * @mcore: Local core (APSS) channel descriptor
 * @ucore: Remote core (TME-L) channel descriptor
 * @rx_pkt: Buffer to pass to client, holds received data from mailbox
 * @mbox_client: Mailbox client for the IPC interrupt
 * @mbox_chan: Mailbox client chan for the IPC interrupt
 * @local_state: Current state of mailbox protocol
 * @link_complete: Use to block until link negotiation with remote proc
 * @ch_complete: Use to block until the channel is fully opened
 * @tx_sent: True if tx is sent and remote proc has not sent ack
 */
struct qmp_device {
	struct udevice *dev;

	void __iomem *mcore_desc;
	void __iomem *ucore_desc;
	union qmp_channel_desc mcore;
	union qmp_channel_desc ucore;

	struct iovec_tmel rx_pkt;

	struct mbox_chan *mbox_chan;

	enum qmp_local_state local_state;

	bool link_complete;
	bool ch_complete;

	atomic_t tx_sent;
};

/**
 * struct tmel - tmel controller instance
 * @dev: The device that corresponds to this mailbox
 * @ctrl: Mailbox controller for use by tmel clients
 * @mdev: qmp_device associated with this tmel instance
 * @pkt: Buffer from client, to be sent over mailbox
 * @ipc_pkt: wrapper used for prepare/un_prepare
 * @sram_dma_addr: mailbox sram address to copy the data
 * @rx_done: Use to indicate receive completion from remote
 * @twork: worker for posting the client req to tmel ctrl
 * @data: client data to be sent for the current request
 */
struct tmel {
	struct udevice *dev;
	struct qmp_device *mdev;
	struct iovec_tmel pkt;
	struct tmel_ipc_pkt *ipc_pkt;
	dma_addr_t sram_dma_addr;
	bool rx_done;
	void *data;
	void __iomem *irq_base;
	u32 irq_num;
	u32 irq_flags;
};

struct tmel_secboot_sec_auth_req {
	u32 sw_id;
	struct tmel_msg_param_type_buf_in elf_buf;
	struct tmel_msg_param_type_buf_in region_list;
	u32 relocate;
};

struct tmel_secboot_sec_auth_resp {
	u32 first_seg_addr;
	u32 first_seg_len;
	u32 entry_addr;
	u32 extended_error;
	u32 status;
};

struct tmel_secboot_sec_auth {
	struct tmel_secboot_sec_auth_req req;
	struct tmel_secboot_sec_auth_resp resp;
};

struct tmel_secboot_sec {
	struct udevice *dev;
	void *elf_buf;
	struct tmel_secboot_sec_auth msg;
};

/**
 * tmel_qmp_send_irq() - send an irq to a remote entity as an event signal.
 * @mdev: Which remote entity that should receive the irq.
 */
static void tmel_qmp_send_irq(struct qmp_device *mdev)
{
	writel(mdev->mcore.val, mdev->mcore_desc);
	/* Ensure desc update is visible before IPC */
	wmb();

	dev_dbg(mdev->dev, "%s: mcore 0x%x ucore 0x%x", __func__,
		mdev->mcore.val, mdev->ucore.val);

	writel(BIT(20), CONFIG_SHARED_IPC_INTERRUPT_REG);
}

/**
 * tmel_qmp_send_data() - Send the data to remote and notify.
 * @mdev: qmp_device to send the data to.
 * @data: Data to be sent to remote processor, should be in the format of
 *	  a iovec_tmel.
 *
 * Copy the data to the channel's mailbox and notify remote subsystem of new
 * data. This function will return an error if the previous message sent has
 * not been read.
 */
static int tmel_qmp_send_data(struct qmp_device *mdev, void *data)
{
	struct iovec_tmel *pkt = (struct iovec_tmel *)data;
	void __iomem *addr;

	if (pkt->iov_len > QMP_MAX_PKT_SIZE) {
		dev_err(mdev->dev, "Unsupported packet size");
		return -EINVAL;
	}

	if (atomic_read(&mdev->tx_sent)) {
		dev_err(mdev->dev, "Tx already sent");
		return -EAGAIN;
	}

	dev_dbg(mdev->dev, "%s: mcore 0x%x ucore 0x%x", __func__,
		mdev->mcore.val, mdev->ucore.val);

	addr = mdev->mcore_desc + QMP_CTRL_DATA_SIZE;
	memcpy(addr, pkt->iov_base, pkt->iov_len);
	wmb();

	mdev->mcore.bits.frag_size = pkt->iov_len;
	mdev->mcore.bits.rem_frag_count = 0;

	dev_dbg(mdev->dev, "Copied buffer to mbox, sz: %d",
		mdev->mcore.bits.frag_size);

	atomic_set(&mdev->tx_sent, 1);

	mdev->mcore.bits.tx = !(mdev->mcore.bits.tx);
	tmel_qmp_send_irq(mdev);

	return 0;
}

/**
 * tmel_qmp_notify_client() - Notify the tmel client about remote data.
 * @tdev: tmel device to notify.
 * @message: response pkt from remote processor, should be in format of iovec_tmel.
 *
 * Wakeup the clients after receiving data from the remote.
 */
static void tmel_qmp_notify_client(struct tmel *tdev, void *message)
{
	struct iovec_tmel *pkt = NULL;

	if (!message) {
		dev_err(tdev->dev, "spurious message received\n");
		return;
	}

	if (tdev->rx_done) {
		dev_err(tdev->dev, "tmel response pending\n");
		return;
	}

	pkt = (struct iovec_tmel *)message;
	tdev->pkt.iov_len = pkt->iov_len;
	tdev->pkt.iov_base = pkt->iov_base;
	tdev->rx_done = true;
}

/**
 * tmel_qmp_recv_data() - Receive data and send ack.
 * @tdev: tmel device that received the notification.
 * @mbox_of: offset of mailbox after QMP Control data.
 *
 * Copies data from mailbox and passes to the client upon receiving data
 * available notification. Also acknowledges the read completion.
 */
static void tmel_qmp_recv_data(struct tmel *tdev, u32 mbox_of)
{
	struct qmp_device *mdev = tdev->mdev;
	void __iomem *addr;
	struct iovec_tmel *pkt;

	addr = mdev->ucore_desc + mbox_of;
	pkt = &mdev->rx_pkt;
	pkt->iov_len = mdev->ucore.bits.frag_size;

	memcpy(pkt->iov_base, addr, pkt->iov_len);
	wmb();
	mdev->mcore.bits.tx_ack = mdev->ucore.bits.tx;
	dev_dbg(mdev->dev, "%s: Send RX data to TMEL Client", __func__);
	tmel_qmp_notify_client(tdev, pkt);

	mdev->mcore.bits.rx_done = !(mdev->mcore.bits.rx_done);
	tmel_qmp_send_irq(mdev);
}

/**
 * tmel_qmp_clr_mcore_ch_state() - Clear the mcore state of a mailbox.
 * @mdev: mailbox device to be initialized.
 */
static void tmel_qmp_clr_mcore_ch_state(struct qmp_device *mdev)
{
	/* Clear all fields except link_state */
	mdev->mcore.bits.ch_state = 0;
	mdev->mcore.bits.ch_state_ack = 0;
	mdev->mcore.bits.tx =  0;
	mdev->mcore.bits.tx_ack =  0;
	mdev->mcore.bits.rx_done = 0;
	mdev->mcore.bits.rx_done_ack = 0;
	mdev->mcore.bits.frag_size = 0;
	mdev->mcore.bits.rem_frag_count = 0;
}

/**
 * tmel_qmp_rx() - Handle incoming messages from remote processor.
 * @tdev: tmel device to send the event to.
 */
static void tmel_qmp_rx(struct tmel *tdev)
{
	struct qmp_device *mdev = tdev->mdev;

	/* read remote_desc from mailbox register */
	mdev->ucore.val = readl(mdev->ucore_desc);

	/* Check if remote link down */
	if (mdev->local_state >= LINK_CONNECTED &&
	    !(mdev->ucore.bits.link_state)) {
		mdev->local_state = LINK_NEGOTIATION;
		mdev->mcore.bits.link_state_ack = mdev->ucore.bits.link_state;
		tmel_qmp_send_irq(mdev);
		return;
	}

	switch (mdev->local_state) {
	case LINK_NEGOTIATION:
		if (!(mdev->mcore.bits.link_state) ||
		    !(mdev->ucore.bits.link_state)) {
			dev_err(mdev->dev, "rx irq:link down state\n");
			break;
		}
		tmel_qmp_clr_mcore_ch_state(mdev);
		mdev->mcore.bits.link_state_ack = mdev->ucore.bits.link_state;
		mdev->local_state = LINK_CONNECTED;
		mdev->link_complete = true;
		dev_dbg(mdev->dev, "Set to link connected");
		break;
	case LINK_CONNECTED:
		/* No need to handle until local opens */
		break;
	case LOCAL_CONNECTING:
		/* Ack to remote ch_state change */
		mdev->mcore.bits.ch_state_ack = mdev->ucore.bits.ch_state;
		mdev->local_state = CHANNEL_CONNECTED;
		mdev->ch_complete = true;
		dev_dbg(mdev->dev, "Set to channel connected");
		tmel_qmp_send_irq(mdev);
		break;
	case CHANNEL_CONNECTED:
		/* Check for remote channel down */
		if (!(mdev->ucore.bits.ch_state)) {
			mdev->local_state = LOCAL_CONNECTING;
			mdev->mcore.bits.ch_state_ack = mdev->ucore.bits.ch_state;
			dev_dbg(mdev->dev, "Remote Disconnect");
			tmel_qmp_send_irq(mdev);
		}

		/* Check TX done */
		if (atomic_read(&mdev->tx_sent) &&
		    mdev->ucore.bits.rx_done != mdev->mcore.bits.rx_done_ack) {
			/* Ack to remote */
			mdev->mcore.bits.rx_done_ack = mdev->ucore.bits.rx_done;
			atomic_set(&mdev->tx_sent, 0);
			dev_dbg(mdev->dev, "TX flag cleared");
		}

		/* Check if remote is Transmitting */
		if (!(mdev->ucore.bits.tx != mdev->mcore.bits.tx_ack))
			break;
		if (mdev->ucore.bits.frag_size == 0 ||
		    mdev->ucore.bits.frag_size > QMP_MAX_PKT_SIZE) {
			dev_err(mdev->dev, "Rx frag size error %d\n",
				mdev->ucore.bits.frag_size);
			break;
		}
		tmel_qmp_recv_data(tdev, QMP_CTRL_DATA_SIZE);
		break;
	case LOCAL_DISCONNECTING:
		if (!(mdev->mcore.bits.ch_state)) {
			tmel_qmp_clr_mcore_ch_state(mdev);
			mdev->local_state = LINK_CONNECTED;
			dev_dbg(mdev->dev, "Channel closed");
			mdev->ch_complete = false;
		}

		break;
	default:
		dev_err(mdev->dev, "Local Channel State corrupted\n");
	}
}

/**
 * tmel_prepare_msg() - copies the payload to the mbox destination
 * @tdev: the tmel device
 * @msg_uid: msg_type/action_id combo
 * @msg_buf: payload to be sent
 * @msg_size: size of the payload
 */
static int tmel_prepare_msg(struct tmel *tdev, u32 msg_uid, void *msg_buf,
			    size_t msg_size)
{
	struct tmel_ipc_pkt *ipc_pkt = tdev->ipc_pkt;
	struct ipc_header *msg_hdr = &ipc_pkt->msg_hdr;
	struct mbox_payload *mbox_payload = &ipc_pkt->payload.mbox_payload;
	struct sram_payload *sram_payload = &ipc_pkt->payload.sram_payload;

	memset(ipc_pkt, 0, sizeof(struct tmel_ipc_pkt));

	msg_hdr->msg_type = TMEL_MSG_UID_MSG_TYPE(msg_uid);
	msg_hdr->action_id = TMEL_MSG_UID_ACTION_ID(msg_uid);

	dev_dbg(tdev->dev, "uid: %d, msg_size: %zu msg_type:%d, action_id:%d\n",
		msg_uid, msg_size, msg_hdr->msg_type, msg_hdr->action_id);

	if (sizeof(struct ipc_header) + msg_size <= QMP_MBOX_IPC_PACKET_SIZE) {
		/* Mbox only */
		msg_hdr->ipc_type = IPC_MBOX_MEM;
		msg_hdr->msg_len = msg_size;
		memcpy((void *)mbox_payload, msg_buf, msg_size);
		wmb();
	} else if (msg_size <= QMP_SRAM_IPC_MAX_BUF_SIZE) {
		/* SRAM */
		msg_hdr->ipc_type = IPC_MBOX_SRAM;
		msg_hdr->msg_len = 8;

		tdev->sram_dma_addr = dma_map_single(msg_buf,
						     msg_size,
						     DMA_BIDIRECTIONAL);

		sram_payload->payload_ptr = tdev->sram_dma_addr;
		sram_payload->payload_len = msg_size;
	} else {
		dev_err(tdev->dev, "Invalid payload length: %zu\n", msg_size);
		return -EINVAL;
	}

	return 0;
}

/**
 * tmel_unprepare_message() - Get the response data back for client
 * @tdev: the tmel device
 * @msg_buf: payload to be sent
 * @msg_size: size of the payload
 */
static void tmel_unprepare_message(struct tmel *tdev, void *msg_buf, size_t msg_size)
{
	struct tmel_ipc_pkt *ipc_pkt = (struct tmel_ipc_pkt *)tdev->pkt.iov_base;
	struct mbox_payload *mbox_payload = &ipc_pkt->payload.mbox_payload;

	if (ipc_pkt->msg_hdr.ipc_type == IPC_MBOX_MEM) {
		memcpy(msg_buf, mbox_payload, msg_size);
		wmb();
	} else if (ipc_pkt->msg_hdr.ipc_type == IPC_MBOX_SRAM) {
		dma_unmap_single(tdev->sram_dma_addr, msg_size, DMA_BIDIRECTIONAL);
		tdev->sram_dma_addr = 0;
	}
}

static u32 read_pending_interrupt(struct tmel *tdev)
{
	u32 shift = tdev->irq_num % 32;
	u32 offset = GICD_ISPENDRn + ((tdev->irq_num / 32) * 4);

	return ((readl(tdev->irq_base + offset) >> shift) & 1);
}

static void clear_pending_interrupt(struct tmel *tdev)
{
	u32 shift = tdev->irq_num % 32;
	u32 offset = GICD_ICPENDRn + ((tdev->irq_num / 32) * 4);
	u32 val = 1 << shift;

	writel(val, tdev->irq_base + offset);
}

static void set_interrupt_flags(struct tmel *tdev)
{
	u32 shift = (tdev->irq_num % 16) * 2;
	u32 offset = GICD_ICFGR + ((tdev->irq_num / 16) * 4);
	u32 val = readl(tdev->irq_base + offset);

	val &= ~(0x3 << shift);
	val |= (tdev->irq_flags == IRQ_TYPE_EDGE_RISING ? 0x2 : 0x0) << shift;
	writel(val, tdev->irq_base + offset);
}

static void enable_interrupt(struct tmel *tdev)
{
	u32 shift = tdev->irq_num % 32;
	u32 offset = GICD_ISENABLERn + ((tdev->irq_num / 32) * 4);
	u32 val = readl(tdev->irq_base + offset) | (1 << shift);

	writel(val, tdev->irq_base + offset);
}

/**
 * tmel_check_for_irq() - Check for interrupt from TMEL.
 * @tdev: tmel device to send the event to.
 */
static void tmel_check_for_irq(struct tmel *tdev)
{
	int status;
	int timeout = 100000;

	do {
		udelay(10);
		status = read_pending_interrupt(tdev);
		if (status) {
			clear_pending_interrupt(tdev);
			break;
		}
		timeout--;
	} while (timeout);

	tmel_qmp_rx(tdev);
}

/**
 * tmel_process_request() - process client msg and wait for response
 * @tdev: the tmel device
 * @msg_uid: msg_type/action_id combo
 * @msg_buf: payload to be sent
 * @msg_size: size of the payload
 */
static int tmel_process_request(struct tmel *tdev, u32 msg_uid, void *msg_buf,
				size_t msg_size)
{
	struct qmp_device *mdev = tdev->mdev;
	struct tmel_ipc_pkt *resp_ipc_pkt;
	int ret = 0;

	if (!msg_buf || !msg_size) {
		dev_err(tdev->dev, "Invalid msg_buf or msg_size\n");
		return -EINVAL;
	}

	tdev->rx_done = false;

	ret = tmel_prepare_msg(tdev, msg_uid, msg_buf, msg_size);
	if (ret)
		return ret;

	tdev->pkt.iov_len = sizeof(struct tmel_ipc_pkt);
	tdev->pkt.iov_base = (void *)tdev->ipc_pkt;

	tmel_qmp_send_data(mdev, &tdev->pkt);

	/*
	 * After sending the data, IRQ will be received from TMEL
	 * to acknowledge it
	 */
	while(!tdev->rx_done)
		tmel_check_for_irq(tdev);

	if (tdev->pkt.iov_len != sizeof(struct tmel_ipc_pkt))
		return -EPROTO;

	resp_ipc_pkt = (struct tmel_ipc_pkt *)tdev->pkt.iov_base;
	tmel_unprepare_message(tdev, msg_buf, msg_size);
	tdev->rx_done = false;

	return resp_ipc_pkt->msg_hdr.response;
}

/**
 * tmel_secboot_sec_auth() - authenticate the remote subsys image
 * @tdev: the tmel device
 * @sw_id: pas_id of the remote
 * @metadata: payload to be sent
 * @size: size of the payload
 */
static int tmel_secboot_sec_auth(struct tmel *tdev, u32 sw_id, void *metadata,
				 size_t size)
{
	struct tmel_secboot_sec *smsg;
	struct udevice *dev = tdev->dev;
	dma_addr_t elf_buf_phys;
	void *elf_buf;
	int ret;

	if (!dev || !metadata)
		return -EINVAL;

	smsg = kzalloc(sizeof(*smsg), GFP_KERNEL);
	if (IS_ERR_OR_NULL(smsg))
		return -ENOMEM;

	elf_buf = dma_alloc_coherent(size, (unsigned long *)&elf_buf_phys);
	if (IS_ERR_OR_NULL(elf_buf)) {
		kfree(smsg);
		return -ENOMEM;
	}

	memcpy(elf_buf, metadata, size);
	wmb();

	smsg->dev = dev;
	smsg->elf_buf = (struct tmel_msg_param_type_buf_in *)&elf_buf;

	smsg->msg.req.sw_id = sw_id;
	smsg->msg.req.elf_buf.buf = (u32)elf_buf_phys;
	smsg->msg.req.elf_buf.buf_len = (u32)size;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_SECBOOT_SEC_AUTH,
				   &smsg->msg,
				   sizeof(struct tmel_secboot_sec_auth));
	if (ret) {
		dev_err(dev, "Failed to send IPC: %d\n", ret);
	} else if (smsg->msg.resp.status) {
		dev_err(dev, "Failed with status: %d", smsg->msg.resp.status);
		ret = smsg->msg.resp.status ? -EINVAL : 0;
	} else if (smsg->msg.resp.extended_error) {
		dev_err(dev, "Failed with error: %d", smsg->msg.resp.extended_error);
		ret = smsg->msg.resp.extended_error ? -EINVAL : 0;
	}

	dma_free_coherent(elf_buf);
	kfree(smsg);

	return ret;
}

int tmelcom_fuse_list_read(struct tmel *tdev, struct tmel_fuse_payload *fuse, size_t size)
{
	int ret;
	struct tmel_fuse_read_multiple_msg msg = {0};
	struct udevice *dev = tdev->dev;
	dma_addr_t dma_fuse;

	if (!dev || !fuse || !size)
		return -EINVAL;

	dma_fuse = dma_map_single(fuse, size, DMA_BIDIRECTIONAL);

	msg.status = TMEL_ERROR_GENERIC;
	msg.fuse_read_data.buf = (u32)dma_fuse;
	msg.fuse_read_data.buf_len = size;

	/*Send Fuse read row IPC call to TME*/
	ret = tmel_process_request(tdev, TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW,
				   &msg, sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);

	dma_unmap_single(dma_fuse, size, DMA_BIDIRECTIONAL);

	return ret ? ret : msg.status;
}

static int tmel_qmp_send(struct mbox_chan *chan, const void *data)
{
	struct tmel *tdev = dev_get_priv(chan->dev);
	int ret = -EINVAL;
	struct tmel_qmp_msg *tmsg = (struct tmel_qmp_msg *)data;

	switch (tmsg->msg_id) {
	case TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW:
		struct tmel_fuse_payload *fuse = (struct tmel_fuse_payload *)tmsg->msg;

		ret = tmelcom_fuse_list_read(tdev, fuse, tmsg->size);
		break;
	case TMEL_MSG_UID_SECBOOT_SEC_AUTH:
		struct tmel_secboot_sec_auth *msg = (struct tmel_secboot_sec_auth *)tmsg->msg;

		ret = tmel_secboot_sec_auth(tdev, msg->req.sw_id,
					    (void *)(uintptr_t)msg->req.elf_buf.buf,
					    msg->req.elf_buf.buf_len);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * tmel_qmp_startup() - Start qmp mailbox channel for communication.
 * @chan: mailbox channel that is being opened.
 * Waits for remote subsystem to open channel if link is not
 * initiated or until timeout.
 */
static int tmel_qmp_startup(struct mbox_chan *chan,
			    struct ofnode_phandle_args *args)
{
	struct tmel *tdev = dev_get_priv(chan->dev);
	struct qmp_device *mdev = tdev->mdev;
	void *rx_buf;

	/*
	 * Kick start the SM from the negotiation phase
	 * Rest of the link changes would follow when remote responds.
	 */
	mdev->mcore.bits.link_state = 1;
	mdev->local_state = LINK_NEGOTIATION;

	rx_buf = devm_kcalloc(chan->dev, 1, QMP_MAX_PKT_SIZE, GFP_KERNEL);
	if (IS_ERR_OR_NULL(rx_buf))
		return -ENOMEM;

	mdev->rx_pkt.iov_base = rx_buf;
	tmel_qmp_send_irq(mdev);

	while (!mdev->link_complete)
		tmel_check_for_irq(tdev);

	if (mdev->local_state == LINK_CONNECTED) {
		mdev->mcore.bits.ch_state = 1;
		mdev->local_state = LOCAL_CONNECTING;
		dev_dbg(mdev->dev, "link complete, local connecting");
		tmel_qmp_send_irq(mdev);
	}

	while (!mdev->ch_complete)
		tmel_check_for_irq(tdev);

	return 0;
}

/**
 * tmel_qmp_shutdown() - Shutdown this mailbox channel.
 * @chan: mailbox channel to be shutdown.
 * Disconnect this mailbox channel so the client does not receive anymore
 * data and can reliquish control of the channel.
 */
static int tmel_qmp_shutdown(struct mbox_chan *chan)
{
	struct qmp_device *mdev = dev_get_priv(chan->dev);

	if (!mdev)
		return -EINVAL;

	if (mdev->local_state != LINK_DISCONNECTED) {
		mdev->local_state = LOCAL_DISCONNECTING;
		mdev->mcore.bits.ch_state = 0;
		tmel_qmp_send_irq(mdev);
	}

	return 0;
}

static struct mbox_ops tmel_qmp_mbox_ops = {
	.of_xlate = tmel_qmp_startup,
	.rfree = tmel_qmp_shutdown,
	.send = tmel_qmp_send,
};

static int tmel_init(struct udevice *dev)
{
	struct tmel *tdev = dev_get_priv(dev);

	tdev->ipc_pkt = devm_kcalloc(dev, 1, sizeof(struct tmel_ipc_pkt),
				     GFP_KERNEL);
	if (IS_ERR_OR_NULL(tdev->ipc_pkt))
		return -ENOMEM;

	tdev->rx_done = false;
	tdev->dev = dev;

	return 0;
}

static struct qmp_device *qmp_init(struct udevice *dev)
{
	struct qmp_device *mdev;
	struct resource res;

	mdev = devm_kcalloc(dev, 1, sizeof(*mdev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mdev))
		return ERR_PTR(-ENOMEM);

	mdev->dev = dev;

	dev_read_resource(dev, 0, &res);
	mdev->mcore_desc = devm_ioremap(dev, res.start, resource_size(&res));
	if (IS_ERR(mdev->mcore_desc))
		return ERR_PTR(-EIO);

	dev_read_resource(dev, 1, &res);
	mdev->ucore_desc = devm_ioremap(dev, res.start, resource_size(&res));
	if (IS_ERR(mdev->ucore_desc))
		return ERR_PTR(-EIO);

	mdev->local_state = LINK_DISCONNECTED;
	mdev->link_complete = false;
	mdev->ch_complete = false;

	return mdev;
}

static int tmel_qmp_parse_dt(struct udevice *dev)
{
	struct tmel *tdev = dev_get_priv(dev);
	const void *fdt = gd->fdt_blob;
	int size, root, addr_cells, intc_node;
	const u32 *list;
	const fdt32_t *ph, *addr_cells_ptr, *reg, *intr_cells;
	u32 phandle, count, irq_type, irq_num;
	u64 addr;

	root = fdt_path_offset(fdt, "/");
	addr_cells_ptr = fdt_getprop(fdt, root, "#address-cells", NULL);
	addr_cells = fdt32_to_cpu(*addr_cells_ptr);

	ph = fdt_getprop(fdt, root, "interrupt-parent", NULL);
	if (!ph) {
		printf("interrupt-parent not found in root\n");
		return -ENODEV;
	}

	phandle = fdt32_to_cpu(*ph);
	intc_node = fdt_node_offset_by_phandle(fdt, phandle);
	if (intc_node < 0) {
		printf("Interrupt controller node not found\n");
		return -ENODEV;
	}

	reg = fdt_getprop(fdt, intc_node, "reg", NULL);
	if (!reg) {
		printf("reg property not found in interrupt controller node\n");
		return -ENODEV;
	}

	if (addr_cells == 2)
		addr = ((u64)fdt32_to_cpu(reg[0]) << 32) | fdt32_to_cpu(reg[1]);
	else
		addr = fdt32_to_cpu(reg[0]);

	tdev->irq_base = (void __iomem *)addr;
	list = dev_read_prop(dev, "interrupts", &size);
	if (!list) {
		printf("\ninterrupts property not found ...\n");
		return -ENOENT;
	}

	intr_cells = fdt_getprop(fdt, intc_node, "#interrupt-cells", NULL);
	if (!intr_cells) {
		printf("\ninterrupts cells property not found ...\n");
		return -ENOENT;
	}

	count = fdt32_to_cpu(*intr_cells);
	if (count != 3)
		return -ENOENT;

	irq_type = fdt32_to_cpu(list[0]);
	irq_num = fdt32_to_cpu(list[1]);
	tdev->irq_flags = fdt32_to_cpu(list[2]);

	if (irq_type == GIC_SPI)
		tdev->irq_num = irq_num + 32;
	else
		tdev->irq_num = irq_num + 16;

	return 0;
}

static int tmel_qmp_mbox_probe(struct udevice *dev)
{
	struct tmel *tdev = dev_get_priv(dev);
	struct qmp_device *mdev;
	int ret;

	ret = tmel_qmp_parse_dt(dev);
	if (ret)
		return -EINVAL;

	ret = tmel_init(dev);
	if (ret)
		return -EINVAL;

	mdev = qmp_init(dev);
	if (IS_ERR(mdev))
		return -EINVAL;

	tdev->mdev = mdev;

	set_interrupt_flags(tdev);
	enable_interrupt(tdev);

	return ret;
}

static const struct udevice_id tmel_qmp_mbox_of_match[] = {
	{ .compatible = "qcom,tmel-qmp-mbox" },
	{}
};

U_BOOT_DRIVER(tmel_qmp_mbox) = {
	.name = "tmel-qmp-mbox",
	.id = UCLASS_MAILBOX,
	.of_match = tmel_qmp_mbox_of_match,
	.probe = tmel_qmp_mbox_probe,
	.priv_auto = sizeof(struct tmel),
	.ops = &tmel_qmp_mbox_ops,
};
