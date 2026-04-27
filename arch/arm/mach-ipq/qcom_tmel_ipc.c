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
#ifdef CONFIG_IPQ_SOFTSKU_SUPPORT
/**
 * ipq_license_install_tme() - Install license via TME IPC
 * @license: Pointer to license blob
 * @licenseLen: Length of license blob
 * @flags: Pointer to flags (output)
 * @identifier: Pointer to identifier buffer (output)
 * @identifierLen: Size of identifier buffer
 * @identifierLenOut: Actual identifier length (output)
 *
 * Return: 0 on success, negative error code on failure
 */
int ipq_license_install_tme(void *license, size_t licenseLen,
			     u64 *flags, u8 *identifier,
			     size_t identifierLen, size_t *identifierLenOut)
{
	int ret;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tmel_license_install_req licenseinstall_req;

	if (!license || !flags || !identifier || !identifierLenOut) {
		printf("Invalid parameters for license install\n");
		return -EINVAL;
	}

	/* Get TMELCOM device */
	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to get TMELCOM device: %d\n", ret);
		return ret;
	}

	/* Prepare request following reference implementation pattern */
	memset(&licenseinstall_req, 0, sizeof(licenseinstall_req));
	licenseinstall_req.status = 1; /* TME_ERROR_GENERIC */
	licenseinstall_req.license_buf.buf = (u32)(uintptr_t)license;
	licenseinstall_req.license_buf.buf_len = licenseLen;
	licenseinstall_req.flags = 0;
	licenseinstall_req.identifier_buf.buf = (u32)(uintptr_t)identifier;
	licenseinstall_req.identifier_buf.buf_len = identifierLen;
	licenseinstall_req.identifier_buf.out_buf_len = 0;

	/* Flush cache for input buffers */
	flush_cache((unsigned long)license, licenseLen);
	flush_cache((unsigned long)identifier, identifierLen);
	flush_cache((unsigned long)&licenseinstall_req, sizeof(licenseinstall_req));

	/* Prepare TME message */
	tmsg.msg = &licenseinstall_req;
	tmsg.msg_id = TME_MSG_UID_QWES_LICENSING_INSTALL;
	tmsg.size = sizeof(licenseinstall_req);

	/* Send message via mailbox */
	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);
	if (ret) {
		printf("Failed to send license install message: %d\n", ret);
		return ret;
	}

	/* Invalidate cache to read response */
	invalidate_dcache_range((unsigned long)&licenseinstall_req,
				(unsigned long)&licenseinstall_req + sizeof(licenseinstall_req));
	invalidate_dcache_range((unsigned long)identifier,
				(unsigned long)identifier + identifierLen);

	/* Check status */
	if (licenseinstall_req.status != 0) {
		printf("License install failed with status: %d\n", licenseinstall_req.status);
		return -EIO;
	}

	/* Copy output parameters */
	*identifierLenOut = licenseinstall_req.identifier_buf.out_buf_len;
	*flags = licenseinstall_req.flags;

	return 0;
}

/**
 * ipq_license_enforce_hw_features_tme() - Enforce HW features via TME IPC
 * @fidBuff: Pointer to feature ID buffer
 * @fidBuffLen: Length of feature ID buffer
 * @fidBuffLenOut: Actual length (output)
 * @HWRegisterInterfaceVersion: HW register interface version (output)
 *
 * Return: 0 on success, negative error code on failure
 */
int ipq_license_enforce_hw_features_tme(struct sec_enforceHWFeatureId *fidBuff,
					size_t fidBuffLen, size_t *fidBuffLenOut,
					u32 *HWRegisterInterfaceVersion)
{
	int ret;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tmel_license_enforce_hw_req enforceFID_req;

	if (!fidBuff || !fidBuffLenOut || !HWRegisterInterfaceVersion) {
		printf("Invalid parameters for HW feature enforcement\n");
		return -EINVAL;
	}

	/* Get TMELCOM device */
	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to get TMELCOM device: %d\n", ret);
		return ret;
	}

	/* Prepare request following reference implementation pattern */
	memset(&enforceFID_req, 0, sizeof(enforceFID_req));
	enforceFID_req.status = 1; /* TME_ERROR_GENERIC */
	enforceFID_req.features_buf.buf = (u32)(uintptr_t)fidBuff;
	enforceFID_req.features_buf.buf_len = fidBuffLen;
	enforceFID_req.features_buf.out_buf_len = 0;
	enforceFID_req.hw_reg_version = 0;

	/* Flush cache for feature buffer */
	flush_cache((unsigned long)fidBuff, fidBuffLen);
	flush_cache((unsigned long)&enforceFID_req, sizeof(enforceFID_req));

	/* Prepare TME message */
	tmsg.msg = &enforceFID_req;
	tmsg.msg_id = TME_MSG_UID_QWES_LICENSING_ENFORCEHWFEATURES;
	tmsg.size = sizeof(enforceFID_req);

	/* Send message via mailbox */
	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);
	if (ret) {
		printf("Failed to send HW feature enforcement message: %d\n", ret);
		return ret;
	}

	/* Invalidate cache to read response */
	invalidate_dcache_range((unsigned long)fidBuff,
				(unsigned long)fidBuff + fidBuffLen);
	invalidate_dcache_range((unsigned long)&enforceFID_req,
				(unsigned long)&enforceFID_req + sizeof(enforceFID_req));

	/* Check status */
	if (enforceFID_req.status != 0) {
		printf("HW feature enforcement failed with status: %d\n", enforceFID_req.status);
		return -EIO;
	}

	/* Copy output parameters */
	*fidBuffLenOut = enforceFID_req.features_buf.out_buf_len;
	*HWRegisterInterfaceVersion = enforceFID_req.hw_reg_version;

	return 0;
}
#endif /* CONFIG_IPQ_SOFTSKU_SUPPORT */

#ifdef CONFIG_CMD_AES_256
/* AES TME implementations */

/* AES 256 encryption TME implementation */
int ipq_aes_256_enc_tme_impl(void *params)
{
	struct aes_256_params *aes_params = params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tmel_aes_encrypt_msg msg __aligned(CONFIG_SYS_CACHELINE_SIZE);
	int ret;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return -ENODEV;
	}

	/* Parse incoming request */
	struct crypto_aes_req_data_t *req =
		(struct crypto_aes_req_data_t *)aes_params->req_ptr;

	/* Build TME encrypt message */
	memset(&msg, 0, sizeof(msg));

	/* Set algorithm */
	if (req->mode == 0)
		msg.req.algo = TME_KAL_AES256_ECB;  /* 0xC */
	else if (req->mode == 1)
		msg.req.algo = TME_KAL_AES256_CBC;  /* 0x8 */
	else {
		printf("Invalid AES mode: %llu\n", (unsigned long long)req->mode);
		return -EINVAL;
	}

	/* Set key handle from derived key */
	msg.req.key_id = (u32)req->key_handle;

	/* Set input plaintext buffer */
	msg.req.in_plain_txt.buf = (u32)req->req_buf;
	msg.req.in_plain_txt.buf_len = (u32)req->req_len;

	/* Set AAD buffer */
	msg.req.in_aad.buf = 0;
	msg.req.in_aad.buf_len = 0;

	/* Set output cipher text buffer */
	msg.resp.out_cipher_txt.buf = (u32)req->resp_buf;
	msg.resp.out_cipher_txt.length = (u32)req->resp_len;
	msg.resp.out_cipher_txt.length_used = 0;

	/* Set output IV buffer (for CBC mode) */
	if (req->mode == 1 && req->ivdata) {  /* CBC mode */
		msg.resp.out_iv.buf = (u32)req->ivdata;
		msg.resp.out_iv.length = (u32)req->iv_len;
		msg.resp.out_iv.length_used = 0;
	}

	/* Flush cache before sending */
	flush_cache((ulong)req->req_buf, req->req_len);
	flush_cache((ulong)&msg, sizeof(msg));

	/* Send via mailbox */
	memset(&tmsg, 0, sizeof(tmsg));
	tmsg.msg_id = TMEL_MSG_UID_AES_ENCRYPT;
	tmsg.msg = &msg;
	tmsg.size = sizeof(msg);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);

	/* Invalidate cache after receiving response */
	invalidate_cache((ulong)&msg, (ulong)&msg + sizeof(msg));
	invalidate_cache((ulong)req->resp_buf, (ulong)req->resp_buf + req->resp_len);

	if (!ret && !msg.resp.status)
		return ret;

	printf("TME encryption failed: ret=%d status=0x%x\n",
	       ret, msg.resp.status);
	return -EIO;
}

/* AES 256 decryption TME implementation */
int ipq_aes_256_dec_tme_impl(void *params)
{
	struct aes_256_params *aes_params = params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tmel_aes_decrypt_msg msg __aligned(CONFIG_SYS_CACHELINE_SIZE);
	int ret;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return -ENODEV;
	}

	/* Parse incoming request */
	struct crypto_aes_req_data_t *req =
		(struct crypto_aes_req_data_t *)aes_params->req_ptr;

	/* Build TME decrypt message */
	memset(&msg, 0, sizeof(msg));

	/* Set algorithm */
	if (req->mode == 0)
		msg.req.algo = TME_KAL_AES256_ECB;  /* 0xC */
	else if (req->mode == 1)
		msg.req.algo = TME_KAL_AES256_CBC;  /* 0x8 */
	else {
		printf("Invalid AES mode: %llu\n", (unsigned long long)req->mode);
		return -EINVAL;
	}

	/* Set key handle from derived key */
	msg.req.key_id = (u32)req->key_handle;

	/* Set input cipher text buffer */
	msg.req.in_cipher_txt.buf = (u32)req->req_buf;
	msg.req.in_cipher_txt.buf_len = (u32)req->req_len;

	/* Set IV buffer (for CBC mode) */
	if (req->mode == 1 && req->ivdata) {  /* CBC mode */
		msg.req.in_iv.buf = (u32)req->ivdata;
		msg.req.in_iv.buf_len = (u32)req->iv_len;
	} else {
		msg.req.in_iv.buf = 0;
		msg.req.in_iv.buf_len = 0;
	}

	/* Set AAD buffer */
	msg.req.in_aad.buf = 0;
	msg.req.in_aad.buf_len = 0;

	/* Set tag buffer (empty for now - used for GCM mode) */
	msg.req.in_tag.buf = 0;
	msg.req.in_tag.buf_len = 0;

	/* Set output plain text buffer */
	msg.resp.out_plain_txt.buf = (u32)req->resp_buf;
	msg.resp.out_plain_txt.length = (u32)req->resp_len;
	msg.resp.out_plain_txt.length_used = 0;

	/* Flush cache before sending */
	flush_cache((ulong)req->req_buf, req->req_len);
	if (req->ivdata && req->iv_len)
		flush_cache((ulong)req->ivdata, req->iv_len);
	flush_cache((ulong)&msg, sizeof(msg));

	/* Send via mailbox */
	memset(&tmsg, 0, sizeof(tmsg));
	tmsg.msg_id = TMEL_MSG_UID_AES_DECRYPT;
	tmsg.msg = &msg;
	tmsg.size = sizeof(msg);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);

	/* Invalidate cache after receiving response */
	invalidate_cache((ulong)&msg, (ulong)&msg + sizeof(msg));
	invalidate_cache((ulong)req->resp_buf, (ulong)req->resp_buf + req->resp_len);

	if (!ret && !msg.resp.status)
		return ret;

	printf("TME decryption failed: ret=%d status=0x%x\n",
	       ret, msg.resp.status);
	return -EIO;
}


#ifdef CONFIG_AES_256_DERIVE_KEY

#define CHIP_RANDOM_BASE_KEY		0x0
#define OEM_PRODUCT_SEED		0x1

/* AES 256 derive key TME implementation */
int ipq_aes_derive_key_tme_impl(void *params)
{
	struct aes_derive_key_params *key_params = params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tme_kdf_spec *kdf;
	struct tme_derive_msg msg __aligned(CONFIG_SYS_CACHELINE_SIZE);
	int ret;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return -ENODEV;
	}

	/* Allocate aligned memory for KDF spec */
	kdf = memalign(ARCH_DMA_MINALIGN, sizeof(*kdf));
	if (!kdf) {
		printf("Memory allocation failed\n");
		return -ENOMEM;
	}

	/* Parse incoming request */
	struct crypto_aes_derive_key_cmd_t_v1 *req =
		(struct crypto_aes_derive_key_cmd_t_v1 *)key_params->req_ptr;

	/* Build KDF spec */
	memset(kdf, 0, sizeof(*kdf));

	/* KDF algorithm */
	kdf->kdf_algo = TME_KAL_KDF_NIST;

	/* PRF digest algorithm */
	kdf->prf_digest_algo = TME_KAL_SHA512_HMAC;

	/* L2 key */
	kdf->l2_key = TME_KID_L2_SECURESTRGSVC;

	/* Set source-dependent parameters */
	switch (req->source) {
	case CHIP_RANDOM_BASE_KEY:
		kdf->input_key = TME_KID_CHIP_RAND_BASE;
		kdf->policy.low = 0x4c204c20;
		kdf->policy.high = 0x84044;
		break;
	case OEM_PRODUCT_SEED:
		kdf->input_key = TME_KID_OEM_PRODUCT_SEED;
		kdf->policy.low = 0xc204c20;
		kdf->policy.high = 0x84048;
		break;
	default:
		printf("Invalid source: 0x%x\n", req->source);
		free(kdf);
		return -EINVAL;
	}

	/* Mix key */
	kdf->mix_key = (u32)req->mixing_key;

	/* Set security context - this is the bindings bitmask */
	kdf->security_context = req->hw_key_bindings.bindings;

	/* Copy context data (salt/label) */
	if (req->hw_key_bindings.context_len > 0 && req->hw_key_bindings.context_len <= 64) {
		memcpy(kdf->sw_context, req->hw_key_bindings.context,
		       req->hw_key_bindings.context_len);
		kdf->sw_context_len = req->hw_key_bindings.context_len;
	}

	/* Build TME message */
	memset(&msg, 0, sizeof(msg));
	msg.req.key_id = TME_KID_ALLOC;
	msg.req.kdf_buf = (u32)(uintptr_t)kdf;
	msg.req.kdf_len = sizeof(*kdf);

	/* Flush cache before sending */
	flush_cache((ulong)kdf, sizeof(*kdf));
	flush_cache((ulong)&msg, sizeof(msg));

	/* Send via mailbox */
	memset(&tmsg, 0, sizeof(tmsg));
	tmsg.msg_id = TMEL_MSG_UID_AES_DERIVE_KEY;
	tmsg.msg = &msg;
	tmsg.size = sizeof(msg);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);

	/* Invalidate cache after receiving response */
	invalidate_cache((ulong)&msg, (ulong)&msg + sizeof(msg));

	if (!ret && !msg.resp.status) {
		*key_params->key_handle = msg.resp.key_id;
		free(kdf);
		return ret;
	}

	printf("TME key derivation failed: ret=%d status=0x%x\n",
	       ret, msg.resp.status);
	free(kdf);
	return -EIO;
}

/* AES 256 derive key with max context TME implementation */
int ipq_aes_derive_key_max_ctxt_tme_impl(void *params)
{
	struct aes_derive_key_max_ctxt_params *key_params = params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tme_kdf_spec *kdf;
	struct tme_derive_msg msg __aligned(CONFIG_SYS_CACHELINE_SIZE);
	int ret;

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return -ENODEV;
	}

	/* Allocate aligned memory for KDF spec (128-byte context) */
	kdf = memalign(ARCH_DMA_MINALIGN, sizeof(*kdf));
	if (!kdf) {
		printf("Memory allocation failed\n");
		return -ENOMEM;
	}

	/* Parse incoming request */
	struct crypto_aes_derive_key_cmd_t_v2 *req =
		(struct crypto_aes_derive_key_cmd_t_v2 *)key_params->req_ptr;

	/* Build KDF spec */
	memset(kdf, 0, sizeof(*kdf));

	/* KDF algorithm */
	kdf->kdf_algo = TME_KAL_KDF_NIST;

	/* PRF digest algorithm */
	kdf->prf_digest_algo = TME_KAL_SHA512_HMAC;

	/* L2 key */
	kdf->l2_key = TME_KID_L2_SECURESTRGSVC;

	/* Set source-dependent parameters */
	switch (req->source) {
	case CHIP_RANDOM_BASE_KEY:
		kdf->input_key = TME_KID_CHIP_RAND_BASE;
		kdf->policy.low = 0x4c204c20;
		kdf->policy.high = 0x84044;
		break;
	case OEM_PRODUCT_SEED:
		kdf->input_key = TME_KID_OEM_PRODUCT_SEED;
		kdf->policy.low = 0xc204c20;
		kdf->policy.high = 0x84048;
		break;
	default:
		printf("Invalid source: 0x%x\n", req->source);
		free(kdf);
		return -EINVAL;
	}

	/* Mix key */
	kdf->mix_key = (u32)req->mixing_key;

	/* Set security context - this is the bindings bitmask */
	kdf->security_context = req->hw_key_bindings.bindings;

	/* Copy context data (salt/label) - supports up to 128 bytes */
	if (req->hw_key_bindings.context_len > 0 &&
	    req->hw_key_bindings.context_len <= TME_KDF_SW_CONTEXT_BYTES_MAX) {
		memcpy(kdf->sw_context, req->hw_key_bindings.context,
		       req->hw_key_bindings.context_len);
		kdf->sw_context_len = req->hw_key_bindings.context_len;
	}

	/* Build TME message */
	memset(&msg, 0, sizeof(msg));
	msg.req.key_id = TME_KID_ALLOC;
	msg.req.kdf_buf = (u32)(uintptr_t)kdf;
	msg.req.kdf_len = sizeof(*kdf);

	/* Flush cache before sending */
	flush_cache((ulong)kdf, sizeof(*kdf));
	flush_cache((ulong)&msg, sizeof(msg));

	/* Send via mailbox */
	memset(&tmsg, 0, sizeof(tmsg));
	tmsg.msg_id = TMEL_MSG_UID_AES_DERIVE_KEY;
	tmsg.msg = &msg;
	tmsg.size = sizeof(msg);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);

	/* Invalidate cache after receiving response */
	invalidate_cache((ulong)&msg, (ulong)&msg + sizeof(msg));

	if (!ret && !msg.resp.status) {
		*key_params->key_handle = msg.resp.key_id;
		free(kdf);
		return ret;
	}

	printf("TME key derivation (max context) failed: ret=%d status=0x%x\n",
	       ret, msg.resp.status);
	free(kdf);
	return -EIO;
}
#endif /* CONFIG_AES_256_DERIVE_KEY */

/* AES clear key TME implementation */
int ipq_aes_clear_key_tme_impl(void *params)
{
	int ret;
	struct aes_clear_key_params *clear_params = (struct aes_clear_key_params *)params;
	struct tmelcom *tmelcom_priv;
	struct tmel_qmp_msg tmsg;
	struct tmel_aes_clear_key_msg msg __aligned(CONFIG_SYS_CACHELINE_SIZE);

	ret = ipq_get_tmelcom_device(&tmelcom_priv);
	if (ret || !tmelcom_priv) {
		printf("Failed to find TMELCOM node %d\n", ret);
		return -ENODEV;
	}

	/* Build TME AES clear key message */
	memset(&msg, 0, sizeof(msg));
	msg.req.key_id = clear_params->key_handle;

	/* Flush cache before sending */
	flush_cache((ulong)&msg, sizeof(msg));

	/* Send via mailbox */
	memset(&tmsg, 0, sizeof(tmsg));
	tmsg.msg_id = TMEL_MSG_UID_AES_CLEAR_KEY;
	tmsg.msg = &msg;
	tmsg.size = sizeof(msg);

	ret = mbox_send(&tmelcom_priv->mbox, &tmsg);

	/* Invalidate cache after receiving response */
	invalidate_cache((ulong)&msg, (ulong)&msg + sizeof(msg));

	if (!ret && !msg.resp.status) {
		printf("AES key = %u cleared successfully\n", clear_params->key_handle);
		return ret;
	}

	printf("TME clear key failed: ret=%d status=0x%x\n", ret, msg.resp.status);
	return -EIO;
}
#endif /* CONFIG_CMD_AES_256 */
