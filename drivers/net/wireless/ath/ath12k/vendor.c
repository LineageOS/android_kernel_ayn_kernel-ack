// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 */

#include <net/netlink.h>
#include <net/mac80211.h>
#include <linux/nl80211-vnd-qca.h>
#include "core.h"
#include "debug.h"
#include "vendor.h"

static int ath12k_vendor_get_supported_features(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct sk_buff *reply_skb;
	struct ath12k *ar;
	int i;
	u16 interface_modes = U16_MAX;
	uint32_t fset = WIFI_FEATURE_INFRA |
			WIFI_FEATURE_INFRA_5G |
			WIFI_FEATURE_SOFT_AP |
			WIFI_FEATURE_HOTSPOT |
			WIFI_FEATURE_AP_STA |
			WIFI_FEATURE_RSSI_MONITOR |
			WIFI_FEATURE_TX_TRANSMIT_POWER |
			WIFI_FEATURE_SET_TX_POWER_LIMIT |
			WIFI_FEATURE_CONFIG_NDO;

	for_each_ar(ah, ar, i)
		interface_modes &= ar->ab->hw_params->interface_modes;

	if ((interface_modes & BIT(NL80211_IFTYPE_P2P_CLIENT)) &&
	    (interface_modes & BIT(NL80211_IFTYPE_P2P_GO)))
		fset |= WIFI_FEATURE_P2P;

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(fset) + NLMSG_HDRLEN);
	if (!reply_skb)
		return -ENOMEM;

	if (nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_FEATURE_SET, fset))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(reply_skb);

nla_put_failure:
	kfree_skb(reply_skb);
	return -EINVAL;
}

const struct nla_policy
ath12k_vendor_get_logger_set_policy[QCA_WLAN_VENDOR_ATTR_LOGGER_MAX + 1] = {
        [QCA_WLAN_VENDOR_ATTR_LOGGER_SUPPORTED] = {.type = NLA_U32},
};

static int ath12k_vendor_get_logger_supp_feature(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data, int data_len)
{
	struct sk_buff *reply_skb;
	uint32_t features = 0;

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(features) + NLA_HDRLEN + NLMSG_HDRLEN);
	if (!reply_skb)
		return -ENOMEM;

	if (nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_LOGGER_SUPPORTED, features))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(reply_skb);

nla_put_failure:
	kfree_skb(reply_skb);
	return -EINVAL;
}

const struct nla_policy
ath12k_vendor_get_wifi_info_policy[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX + 1] = {
        [QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION] = {.type = NLA_U8 },
        [QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION] = {.type = NLA_U8 },
};

#define ATH12K_VERSIONSTR "6.18-ATH12K"
#define ATH12K_FW_VERSIONSTR "UNKNOWN"
static int ath12k_get_wifi_info(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int data_len)
{
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX + 1];
	struct sk_buff *reply_skb;
	uint32_t skb_len = 0;

        if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX,
		      data, data_len, ath12k_vendor_get_wifi_info_policy, NULL))
		return -EINVAL;

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION])
		skb_len += nla_total_size(strlen(ATH12K_VERSIONSTR) + 1);

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION])
		skb_len += nla_total_size(strlen(ATH12K_FW_VERSIONSTR) + 1);

	if (!skb_len)
		return -EINVAL;

	skb_len += NLMSG_HDRLEN;
	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, skb_len);
	if (!reply_skb)
		return -ENOMEM;

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION] &&
	    nla_put_string(reply_skb, QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION,
			   ATH12K_VERSIONSTR))
		goto error_nla_fail;

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION] &&
	    nla_put_string(reply_skb, QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION,
			   ATH12K_FW_VERSIONSTR))
		goto error_nla_fail;

	return cfg80211_vendor_cmd_reply(reply_skb);

error_nla_fail:
	kfree_skb(reply_skb);
	return -EINVAL;
}

static struct wiphy_vendor_command ath12k_vendor_commands[] = {
	{
		.info = {
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_SUPPORTED_FEATURES,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = ath12k_vendor_get_supported_features,
		.policy = VENDOR_CMD_RAW_DATA
	},
	{
		.info = {
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_LOGGER_FEATURE_SET,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = ath12k_vendor_get_logger_supp_feature,
		.policy = ath12k_vendor_get_logger_set_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_LOGGER_MAX
	},
	{
		.info = {
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = ath12k_get_wifi_info,
		.policy = ath12k_vendor_get_wifi_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX
	},
};

int ath12k_vendor_register(struct ieee80211_hw *hw)
{
	hw->wiphy->vendor_commands = ath12k_vendor_commands;
	hw->wiphy->n_vendor_commands = ARRAY_SIZE(ath12k_vendor_commands);

	return 0;
}

