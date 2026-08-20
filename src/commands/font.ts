import { invoke } from "@tauri-apps/api/core";

/** 内置字体族名（与 Rust 端 FONT_FAMILY_NAME 一致） */
export const BUILTIN_FONT_FAMILY = "Aa言念君子 温其如玉";

/** 检查字体是否已安装 */
export const isFontInstalled = async (fontName: string = BUILTIN_FONT_FAMILY) => {
	const result = await invoke<boolean>("is_font_installed", {
		fontName,
	});
	return result;
};

/** 安装内置字体到用户级（无需管理员） */
export const installFont = async () => {
	const result = await invoke<void>("install_font");
	return result;
};
