// SPDX-License-Identifier: GPL-2.0+
/*
 * TME Communication Implementation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <mailbox.h>
#include <linux/tmelcom-qmp.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <mach/ipq.h>
#include <cpu_func.h>

/*
 * Invalidate range from all levels of d-cache/unified-cache.
 */
void invalidate_cache(ulong start, ulong end)
{
	start = ALIGN_DOWN((ulong)start, CONFIG_SYS_CACHELINE_SIZE);
	end = ALIGN((ulong)end, CONFIG_SYS_CACHELINE_SIZE);

	invalidate_dcache_range(start, end);
}

/* TME list_fuse implementation */

int ipq_list_fuse_tme_impl(void *params)
{
	int ret;
	struct list_fuse_params *fuse_params = (struct list_fuse_params *)params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return CMD_RET_FAILURE;
	}

	/* Build fuse read message directly */
	memset(&tmsg, 0, sizeof(struct tmel_qmp_msg));
	tmsg.msg_id = TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW;
	tmsg.msg = (void *)fuse_params->fuse;
	tmsg.size = fuse_params->fuse_payload_size * fuse_params->fuse_read_cnt;
	flush_cache((unsigned long)fuse_params->fuse, fuse_params->size);
	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);
	invalidate_dcache_range((unsigned long)fuse_params->fuse,
				(unsigned long)fuse_params->fuse +
				fuse_params->size);
	if (ret)
		debug("Failed to send TME mailbox message: %d\n", ret);

	return ret;
}

/* TME dump_fuse implementation */
int ipq_dump_fuse_tme_impl(void *params)
{
	int ret;
	struct dump_fuse_params *fuse_params = (struct dump_fuse_params *)params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return CMD_RET_FAILURE;
	}

	/* Build fuse read message directly */
	memset(&tmsg, 0, sizeof(struct tmel_qmp_msg));
	tmsg.msg_id = TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW;
	tmsg.msg = (void *)fuse_params->fuse;
	tmsg.size = fuse_params->fuse_payload_size * fuse_params->fuse_read_cnt;
	flush_cache((unsigned long)fuse_params->fuse, fuse_params->size);
	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);
	invalidate_dcache_range((unsigned long)fuse_params->fuse,
				(unsigned long)fuse_params->fuse +
				fuse_params->size);
	if (ret)
		debug("Failed to send TME mailbox message: %d\n", ret);

	return ret;
}

/* TME check_secure_boot implementation */
int ipq_check_secure_boot_tme_impl(void *params)
{
	int ret;
	struct check_secure_boot_params *boot_params = (struct check_secure_boot_params *)params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return CMD_RET_FAILURE;
	}

	/* Build fuse read message to read secure boot fuse */
	memset(&tmsg, 0, sizeof(struct tmel_qmp_msg));
	tmsg.msg_id = TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW;
	tmsg.msg = (void *)boot_params->fuse;
	tmsg.size = boot_params->fuse_payload_size;
	flush_dcache_range((unsigned long)boot_params->fuse,
			   (unsigned long)boot_params->fuse + boot_params->size);
	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);
	invalidate_dcache_range((unsigned long)boot_params->fuse,
				(unsigned long)boot_params->fuse +
				boot_params->size);
	if (ret) {
		debug("Failed to send TME mailbox message: %d\n", ret);
		return ret;
	}

	/* Check the fuse value for secure boot enable bit */
	if (boot_params->fuse[0].lsb_val & OEM_SEC_BOOT_ENABLE)
		*boot_params->result = true;

	return 0;
}

#ifdef CONFIG_SECURE_AUTH_V2
static int ipq_secure_auth_tme_v2(void *params)
{
	int ret;
	struct secure_auth_params *auth_params = (struct secure_auth_params *)params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tmel_sec_auth smsg;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return CMD_RET_FAILURE;
	}

	/* Build TME V2 message payload directly */
	memset(&tmsg, 0, sizeof(struct tmel_qmp_msg));
	memset(&smsg, 0, sizeof(struct tmel_sec_auth));
	tmsg.msg_id = TMEL_MSG_UID_SECBOOT_SEC_AUTH;
	smsg.sw_id = auth_params->type;
	smsg.elf_buf.buf = auth_params->addr;
	smsg.elf_buf.buf_len = auth_params->size;
	smsg.region_list.buf = (uintptr_t)auth_params->load_seg_buff;
	smsg.region_list.buf_len = auth_params->load_seg_cnt;
	smsg.relocate = auth_params->relocate;
	tmsg.msg = &smsg;

	if (auth_params->load_seg_buff)
		flush_cache((unsigned long)auth_params->load_seg_buff,
			    (unsigned long)(auth_params->load_seg_info_size *
			    auth_params->load_seg_cnt));

	if (auth_params->addr)
		flush_cache((unsigned long)auth_params->addr,
			    (unsigned long)auth_params->size);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);
	invalidate_cache((unsigned long)auth_params->addr,
				(unsigned long)auth_params->addr + auth_params->size);

	return ret;
}
#endif /* CONFIG_SECURE_AUTH_V2 */

#ifdef CONFIG_SECURE_AUTH_V3
static int ipq_secure_auth_tme_v3(void *params)
{
	int ret;
	struct secure_auth_params *auth_params = (struct secure_auth_params *)params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tmel_sec_auth_v2 smsg;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return CMD_RET_FAILURE;
	}

	/* Build TME V3 message payload directly */
	memset(&tmsg, 0, sizeof(struct tmel_qmp_msg));
	memset(&smsg, 0, sizeof(struct tmel_sec_auth_v2));

	tmsg.msg_id = TMEL_MSG_UID_SECBOOT_SEC_AUTH_V2;
	smsg.sw_id = auth_params->type;
	smsg.elf_buf.buf = auth_params->addr;
	smsg.elf_buf.buf_len = auth_params->size;
	smsg.region_list.buf = (uintptr_t)auth_params->load_seg_buff;
	smsg.region_list.buf_len = auth_params->load_seg_cnt;
	smsg.relocate = auth_params->relocate;
	smsg.nsIntegrityCheck = auth_params->flags;
	tmsg.msg = &smsg;

	if (auth_params->load_seg_buff)
		flush_cache((unsigned long)auth_params->load_seg_buff,
			    (unsigned long)(auth_params->load_seg_info_size *
			    auth_params->load_seg_cnt));

	if (auth_params->addr)
		flush_cache((unsigned long)auth_params->addr,
			    (unsigned long)auth_params->size);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);
	invalidate_cache((unsigned long)auth_params->addr,
				(unsigned long)auth_params->addr + auth_params->size);
	if (!ret)
		printf("KeyHandle : 0x%X\n", smsg.keyHandle);

	return ret;
}
#endif  /* CONFIG_SECURE_AUTH_V3 */

int ipq_secure_auth_tme_impl(void *params)
{
#ifdef CONFIG_SECURE_AUTH_V2
	return ipq_secure_auth_tme_v2(params);
#elif defined(CONFIG_SECURE_AUTH_V3)
	return ipq_secure_auth_tme_v3(params);
#else
	printf("Error: No secure auth version configured\n");
	return CMD_RET_FAILURE;
#endif
}

/* TME get_version implementation */
int ipq_get_tme_version_impl(void *params)
{
	int ret;
	struct tmel_get_tme_version *version_params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;

	/* Validate params before using */
	if (!params) {
		printf("Error: Invalid params pointer\n");
		return -EINVAL;
	}

	version_params = (struct tmel_get_tme_version *)params;

	/* Validate version_params members */
	if (!version_params->pdata) {
		printf("Error: Invalid pdata pointer\n");
		return -EINVAL;
	}

	if (version_params->length == 0) {
		printf("Error: Invalid length value\n");
		return -EINVAL;
	}

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return -ENODEV;
	}

	/* Build TME get version message */
	memset(&tmsg, 0, sizeof(struct tmel_qmp_msg));
	tmsg.msg_id = TMEL_MSG_UID_SECBOOT_GET_STATE;
	tmsg.msg = version_params;
	tmsg.size = sizeof(struct tmel_get_tme_version);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);

	/* Invalidate cache after receiving */
	invalidate_dcache_range((unsigned long)version_params->pdata,
				(unsigned long)version_params->pdata +
				version_params->length);

	if (ret)
		debug("Failed to send TME mailbox message: %d\n", ret);

	return ret;
}

#ifdef CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT
/* TME PRNG get implementation */
int ipq_prng_get_tme_impl(void *params)
{
	int ret;
	struct tmel_get_prng *prng_params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;

	/* Validate params before using */
	if (!params) {
		printf("Error: Invalid params pointer\n");
		return -EINVAL;
	}

	prng_params = (struct tmel_get_prng *)params;

	/* Validate prng_params members */
	if (!prng_params->pdata) {
		printf("Error: Invalid pdata pointer\n");
		return -EINVAL;
	}

	if (prng_params->length == 0) {
		printf("Error: Invalid length value\n");
		return -EINVAL;
	}

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return -ENODEV;
	}

	/* Build TME PRNG get message */
	memset(&tmsg, 0, sizeof(struct tmel_qmp_msg));
	tmsg.msg_id = TMEL_MSG_UID_HCS_PRNG_GET;
	tmsg.msg = prng_params;
	tmsg.size = sizeof(struct tmel_get_prng);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);

	/* Invalidate cache after receiving */
	invalidate_dcache_range((unsigned long)prng_params->pdata,
				(unsigned long)prng_params->pdata +
				prng_params->length);

	if (ret)
		debug("Failed to send TME mailbox message: %d\n", ret);

	return ret;
}
#endif /* CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT */
