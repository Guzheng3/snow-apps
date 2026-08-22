import { join as joinPath } from "@tauri-apps/api/path";
import { fetch } from "@tauri-apps/plugin-http";
import { createDir, getAppConfigBaseDir, writeFile } from "@/commands/file";
import { randomString } from "@/utils/random";

const IMAGE_DIRECTORY = "sTranslateImages";

const canvasToPng = async (canvas: HTMLCanvasElement) => {
	const blob = await new Promise<Blob | null>((resolve) => {
		canvas.toBlob(resolve, "image/png", 1);
	});

	if (!blob) {
		throw new Error("无法导出截图 PNG");
	}

	return blob.arrayBuffer();
};

const getExternalCallUrl = (port: number) => {
	if (!Number.isInteger(port) || port < 1 || port > 65535) {
		throw new Error("STranslate 外部调用端口无效");
	}

	return `http://127.0.0.1:${port}/translate_ocr_image`;
};

/**
 * 将截图保存为本地 PNG 并交给 STranslate 的外部调用服务。
 * STranslate 的图片接口只接受可由其进程读取的文件路径。
 */
export const translateCanvasWithSTranslate = async (
	canvas: HTMLCanvasElement,
	port: number,
) => {
	const imageDirectory = await joinPath(
		await getAppConfigBaseDir(),
		IMAGE_DIRECTORY,
	);
	await createDir(imageDirectory);

	const imagePath = await joinPath(
		imageDirectory,
		`capture-${Date.now()}-${randomString(8)}.png`,
	);
	await writeFile(imagePath, await canvasToPng(canvas));

	let response: Response;
	try {
		response = await fetch(getExternalCallUrl(port), {
			method: "POST",
			headers: { "Content-Type": "text/plain; charset=utf-8" },
			body: imagePath,
		});
	} catch {
		throw new Error(
			"无法连接 STranslate：请启动 STranslate，并在“网络设置”启用外部调用。",
		);
	}

	if (!response.ok) {
		throw new Error(`STranslate 外部调用失败（HTTP ${response.status}）`);
	}

	const result = (await response.json()) as {
		code?: number;
		data?: string;
	};
	if (result.code !== 200) {
		throw new Error(result.data || "STranslate 未接受截图翻译请求");
	}
};
