import { invoke } from "@tauri-apps/api/core";

export const translateLocalInit = async (modelDir: string) => {
	return await invoke("translate_local_init", { modelDir });
};

export const translateLocalRelease = async () => {
	await invoke("translate_local_release");
};

export const translateLocalText = async (text: string) => {
	return await invoke<string>("translate_local_text", { text });
};
