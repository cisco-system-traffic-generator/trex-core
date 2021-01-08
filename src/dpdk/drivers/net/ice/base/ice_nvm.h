/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2019
 */

#ifndef _ICE_NVM_H_
#define _ICE_NVM_H_

#define ICE_NVM_CMD_READ		0x0000000B
#define ICE_NVM_CMD_WRITE		0x0000000C

/* NVM Access config bits */
#define ICE_NVM_CFG_MODULE_M		MAKEMASK(0xFF, 0)
#define ICE_NVM_CFG_MODULE_S		0
#define ICE_NVM_CFG_FLAGS_M		MAKEMASK(0xF, 8)
#define ICE_NVM_CFG_FLAGS_S		8
#define ICE_NVM_CFG_EXT_FLAGS_M		MAKEMASK(0xF, 12)
#define ICE_NVM_CFG_EXT_FLAGS_S		12
#define ICE_NVM_CFG_ADAPTER_INFO_M	MAKEMASK(0xFFFF, 16)
#define ICE_NVM_CFG_ADAPTER_INFO_S	16

/* NVM Read Get Driver Features */
#define ICE_NVM_GET_FEATURES_MODULE	0xE
#define ICE_NVM_GET_FEATURES_FLAGS	0xF

/* NVM Read/Write Mapped Space */
#define ICE_NVM_REG_RW_MODULE	0x0
#define ICE_NVM_REG_RW_FLAGS	0x1

#define ICE_NVM_ACCESS_MAJOR_VER	0
#define ICE_NVM_ACCESS_MINOR_VER	5

/* NVM Access feature flags. Other bits in the features field are reserved and
 * should be set to zero when reporting the ice_nvm_features structure.
 */
#define ICE_NVM_FEATURES_0_REG_ACCESS	BIT(1)

/* NVM Access Features */
struct ice_nvm_features {
	u8 major;		/* Major version (informational only) */
	u8 minor;		/* Minor version (informational only) */
	u16 size;		/* size of ice_nvm_features structure */
	u8 features[12];	/* Array of feature bits */
};

/* NVM Access command */
struct ice_nvm_access_cmd {
	u32 command;		/* NVM command: READ or WRITE */
	u32 config;		/* NVM command configuration */
	u32 offset;		/* offset to read/write, in bytes */
	u32 data_size;		/* size of data field, in bytes */
};

/* NVM Access data */
union ice_nvm_access_data {
	u32 regval;	/* Storage for register value */
	struct ice_nvm_features drv_features; /* NVM features */
};

/* NVM Access registers */
#define GL_HIDA(_i)			(0x00082000 + ((_i) * 4))
#define GL_HIBA(_i)			(0x00081000 + ((_i) * 4))
#define GL_HICR				0x00082040
#define GL_HICR_EN			0x00082044
#define GLGEN_CSR_DEBUG_C		0x00075750
#define GLPCI_LBARCTRL			0x0009DE74
#define GLNVM_GENS			0x000B6100
#define GLNVM_FLA			0x000B6108

#define ICE_NVM_ACCESS_GL_HIDA_MAX	15
#define ICE_NVM_ACCESS_GL_HIBA_MAX	1023

u32 ice_nvm_access_get_module(struct ice_nvm_access_cmd *cmd);
u32 ice_nvm_access_get_flags(struct ice_nvm_access_cmd *cmd);
u32 ice_nvm_access_get_adapter(struct ice_nvm_access_cmd *cmd);
enum ice_status
ice_nvm_access_read(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		    union ice_nvm_access_data *data);
enum ice_status
ice_nvm_access_write(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		     union ice_nvm_access_data *data);
enum ice_status
ice_nvm_access_get_features(struct ice_nvm_access_cmd *cmd,
			    union ice_nvm_access_data *data);
enum ice_status
ice_handle_nvm_access(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		      union ice_nvm_access_data *data);
enum ice_status ice_init_nvm(struct ice_hw *hw);
enum ice_status ice_read_sr_word(struct ice_hw *hw, u16 offset, u16 *data);
enum ice_status
ice_read_sr_buf(struct ice_hw *hw, u16 offset, u16 *words, u16 *data);
#endif /* _ICE_NVM_H_ */
