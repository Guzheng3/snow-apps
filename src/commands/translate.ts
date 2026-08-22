import { invoke } from "@tauri-apps/api/core";
import type { CloudTranslationConfig } from "@/types/appSettings";

/** 云翻译（使用内置引擎，按优先级自动 fallback） */
export const cloudTranslate = async (
	text: string,
	from: string,
	to: string,
): Promise<string> => {
	return await invoke<string>("cloud_translate", { text, from, to });
};

/** 用指定引擎测试翻译 */
export const cloudTranslateTest = async (
	engineId: string,
	text: string,
	from: string,
	to: string,
): Promise<string> => {
	return await invoke<string>("cloud_translate_test", {
		engineId,
		text,
		from,
		to,
	});
};

/** 更新云翻译配置 */
export const cloudTranslateSetConfig = async (
	config: CloudTranslationConfig,
): Promise<void> => {
	await invoke("cloud_translate_set_config", {
		configJson: JSON.stringify(config),
	});
};

/** 获取可用引擎列表 */
export const cloudTranslateGetEngines = async () => {
	return await invoke<
		{ id: string; name: string; needsKey: boolean; isFree: boolean }[]
	>("cloud_translate_get_engines");
};