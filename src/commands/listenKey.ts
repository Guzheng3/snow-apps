import { invoke } from "@tauri-apps/api/core";
import { getPlatform } from "@/utils/platform";

export const listenKeyStart = async () => {
	// macOS 下 Ctrl、Shift、Command 等键浏览器不会响应，特殊处理下
	if (getPlatform() !== "macos") {
		return;
	}

	const result = await invoke<void>("listen_key_start");
	return result;
};

export const listenKeyStop = async () => {
	if (getPlatform() !== "macos") {
		return;
	}

	const result = await invoke<void>("listen_key_stop");
	return result;
};

export const listenKeyStopByWindowLabel = async (windowLabel: string) => {
	if (getPlatform() !== "macos") {
		return;
	}

	const result = await invoke<void>("listen_key_stop_by_window_label", {
		windowLabel,
	});
	return result;
};

export const listenMouseStart = async () => {
	const result = await invoke<void>("listen_mouse_start");
	return result;
};

export const listenMouseStop = async () => {
	const result = await invoke<void>("listen_mouse_stop");
	return result;
};

export const listenMouseStopByWindowLabel = async (windowLabel: string) => {
	if (getPlatform() !== "macos") {
		return;
	}

	const result = await invoke<void>("listen_mouse_stop_by_window_label", {
		windowLabel,
	});
	return result;
};
