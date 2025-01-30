// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/types.h>

/*
 * Global and constant
 */
#define BOARD_DTS_MAX_NAMELEN				32
extern struct multidtb_config *g_board_dtb_info;
/*
 * Structure and static
 */
#ifdef CONFIG_DTB_RESELECT
struct machid_dts_map
{
	int machid;
	char* dts;
};

struct multidtb_config {
	struct machid_dts_map *list;
	int ncount;
	char dts_base[BOARD_DTS_MAX_NAMELEN];
	char dts_name[BOARD_DTS_MAX_NAMELEN];
};
#endif /* CONFIG_DTB_RESELECT */
/*
 * Function declaration
 */
void ipq_update_board_name(int machid, struct multidtb_config *dtb);
void ipq_board_early_init_f(void);
