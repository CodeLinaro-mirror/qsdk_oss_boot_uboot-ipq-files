/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _LINUX_TMELCOM_QMP_H
#define _LINUX_TMELCOM_QMP_H
#include <linux/bitfield.h>
#include <dt-bindings/interrupt-controller/irq.h>

/*
 * Macro used to define unique TMEL Message Identifier based on
 * message type and action identifier.
 */
#define MSGTYPE_MASK GENMASK(15, 8)
#define ACTIONID_MASK GENMASK(7, 0)

#define TMEL_MSG_UID_CREATE(m, a) ((u32)((((m) & 0xff) << 8) | ((a) & 0xff)))

#define TMEL_ACTION_LOOPBACK_TEST_MBOX_ADD_VAL                  0x01
#define TMEL_MSG_LOOPBACK_TEST           0xFF
/*
 * Helper macro to extract the messageType from TMEL_MSG_UID
 */
#define TMEL_MSG_UID_MSG_TYPE(v)	FIELD_GET(MSGTYPE_MASK, v)

/*
 * Helper macro to extract the actionID from TMEL_MSG_UID
 */
#define TMEL_MSG_UID_ACTION_ID(v)	FIELD_GET(ACTIONID_MASK, v)

/*
 * All definitions of supported messageTypes.
 */
#define TMEL_MSG_SECBOOT	0x00
#define TMEL_MSG_FUSE           0x03

/*
 * Action IDs for TMEL_MSG_SECBOOT
 */
#define TMEL_ACTION_SECBOOT_SEC_AUTH		0x04
#define TMEL_ACTION_SECBOOT_SEC_AUTH_V2		0x0F
#define TMEL_ACTION_SECBOOT_SS_TEAR_DOWN	0x0a
#define TMEL_ACTION_SECBOOT_GET_STATE		0x0C

/*
 *   Action ID's for TMEL_MSG_FUSE
 */
#define TMEL_ACTION_FUSE_READ_SINGLE                     0x00    /* Deprecated */
#define TMEL_ACTION_FUSE_READ_MULTIPLE                   0x01    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_SINGLE                    0x02    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_MULTIPLE                  0x03    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_SECURE                    0x04    /* Deprecated */
#define TMEL_ACTION_FUSE_READ_SINGLE_ROW                 0x05
#define TMEL_ACTION_FUSE_READ_MULTIPLE_ROW               0x06
#define TMEL_ACTION_FUSE_WRITE_SINGLE_ROW                0x07
#define TMEL_ACTION_FUSE_WRITE_MULTIPLE_ROW              0x08
#define TMEL_ACTION_FUSE_ROM_PATCH_REQ                   0x09

/*
 * UIDs for TMEL_MSG_SECBOOT
 */
#define TMEL_MSG_UID_SECBOOT_SEC_AUTH	TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
					TMEL_ACTION_SECBOOT_SEC_AUTH)

#define TMEL_MSG_UID_SECBOOT_SEC_AUTH_V2	TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
						TMEL_ACTION_SECBOOT_SEC_AUTH_V2)

#define TMEL_MSG_UID_SECBOOT_SS_TEAR_DOWN	TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
						TMEL_ACTION_SECBOOT_SS_TEAR_DOWN)

#define TMEL_MSG_UID_SECBOOT_GET_STATE		TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
						TMEL_ACTION_SECBOOT_GET_STATE)

#define TMEL_MSG_UID_LOOPBACK_TEST_MBOX_ADD_VAL	TMEL_MSG_UID_CREATE(TMEL_MSG_LOOPBACK_TEST,\
						TMEL_ACTION_LOOPBACK_TEST_MBOX_ADD_VAL)

/*
 * Read multiple fuses
 */
#define TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW	TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
						TMEL_ACTION_FUSE_READ_MULTIPLE_ROW)

#define TMEL_MAX_FUSE_ADDR_SIZE 8

struct tmel_qmp_msg {
	void *msg;
	u32 msg_id;
	size_t size;
};

struct tmel_add_val {
	u32 val1;
	u32 val2;
};

struct tmel_msg_param_type_buf_in {
	u32 buf;
	u32 buf_len;
};

struct tmel_msg_param_type_buf_out {
	u32 buf;
	u32 buf_len;
	u32 out_buf_len;
};

struct tmel_msg_param_type_buf_in_out {
	u32 buf;
	u32 buf_len;
	u32 out_buf_len;
};

struct tmel_sec_auth {
	u32 sw_id;
	struct tmel_msg_param_type_buf_in elf_buf;
	struct tmel_msg_param_type_buf_in region_list;
	u32 relocate;
};

struct tmel_sec_auth_v2 {
	u32 sw_id;
	struct tmel_msg_param_type_buf_in elf_buf;
	struct tmel_msg_param_type_buf_in region_list;
	u32 relocate;
	u32 nsIntegrityCheck:1;
	u32 reservedBits:31;
	struct tmel_msg_param_type_buf_in reservedBuf;
	u32 keyHandle;
};

struct tmel_fuse_payload {
	u32 fuse_addr;
	u32 lsb_val;
	u32 msb_val;
} __packed;

struct tmel_fuse_read_multiple_msg {
	u32 status;
	struct tmel_msg_param_type_buf_in_out fuse_read_data;
} __packed;

struct tmelcom {
	struct mbox_chan mbox;
};

/*
 * TME PATCH VERSION structures
 */

/*
 * TME Response Buffer structure
 * Matches TMEResponseCBuffer from TME firmware
 */
struct tmel_response_buffer {
	u32 pdata;              /* Pointer to the buffer */
	u32 length;             /* Length of the buffer */
	u32 length_used;        /* Actual length of the buffer used */
} __packed;

/*
 * TME Patch Version Length
 */
#define TME_PATCH_VERSION_LENGTH        128

/*
 * TME State Type structure
 * Matches tmeStateType_t from TME firmware
 */
struct tmel_state_type {
	u32 tme_patch_status : 1;       /* Only 1 bit tmePatchStatus_t value */
	u32 tme_mode : 1;               /* Only 1 bit tmeModeType_t value */
	u32 reserve : 30;               /* 30 bits reserved */
} __packed;

/*
 * Get TME State Response structure
 * Matches tmeGetStateRsp_t from TME firmware
 */
struct tmel_get_state_rsp {
	u32 state;                      /* Get TMEL State */
	struct tmel_response_buffer patch_version;      /* TMEL Patch version buffer */
	u32 status;                                     /* TME Get State Status */
} __packed;

/*
 * Get TME State structure
 * Matches tmeGetState_t from TME firmware
 */
struct tmel_get_state {
	struct tmel_get_state_rsp rsp;
} __packed;

/*
 * Simplified TME Get State structure for internal use
 * Used when calling tmel_qmp_msg and tmel_qmp_send
 */
struct tmel_get_tme_version {
	u32 pdata;              /* Pointer to patch version buffer */
	u32 length;             /* Length of the buffer (should be >= TME_PATCH_VERSION_LENGTH) */
} __packed;

void tmel_secboot_sec_free(void *ptr);
int ipq_get_tmelcom_device(struct tmelcom **tmelcom_priv);
int ipq_list_fuse_tme_impl(void *params);
int ipq_dump_fuse_tme_impl(void *params);
int ipq_check_secure_boot_tme_impl(void *params);
int ipq_secure_auth_tme_impl(void *params);
int ipq_get_tme_version_impl(void *params);

#endif  /* _LINUX_TMELCOM_QMP_H */
