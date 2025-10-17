import { isAdmin } from "@/commands/core";
import { getAppConfigBaseDir, getAppConfigDir } from "@/commands/file";

let isAdminResultCache: boolean | undefined;
export const isAdminWithCache = async () => {
	if (isAdminResultCache !== undefined) {
		return isAdminResultCache;
	}

	const result = await isAdmin();
	isAdminResultCache = result;
	return result;
};

/**
 * 判断是否支持 OffscreenCanvas
 * @returns 是否支持
 */
export const supportOffscreenCanvas = () => {
	if (typeof window === "undefined") {
		return false;
	}

	return "OffscreenCanvas" in window;
};

let getAppConfigBaseDirCache: string | undefined;
export const getAppConfigBaseDirWithCache = async () => {
	if (getAppConfigBaseDirCache !== undefined) {
		return getAppConfigBaseDirCache;
	}

	getAppConfigBaseDirCache = await getAppConfigBaseDir();
	return getAppConfigBaseDirCache;
};

let configDirPathCache: string | undefined;
export const getConfigDirPath = async () => {
	if (configDirPathCache) {
		return configDirPathCache;
	}

	configDirPathCache = await getAppConfigDir();

	return configDirPathCache;
};

export function listenDevicePixelRatio(callback: (ratio: number) => void) {
	const media = window.matchMedia(
		`(resolution: ${window.devicePixelRatio}dppx)`,
	);

	function handleChange() {
		callback(window.devicePixelRatio);
	}

	media.addEventListener("change", handleChange);

	return function stopListen() {
		media.removeEventListener("change", handleChange);
	};
}
