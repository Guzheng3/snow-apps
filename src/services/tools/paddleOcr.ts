import { fetch } from "@tauri-apps/plugin-http";
import type { AppSettingsData, AppSettingsGroup } from "@/types/appSettings";
import type { OcrDetectResult } from "@/types/commands/ocr";
import { appError } from "@/utils/log";

export interface PaddleOcrSettings {
	apiUrl: string;
	token: string;
	model: string;
}

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

/**
 * 从 PaddleOCR 返回的 jsonl 文本中提取 markdown 文本
 */
const extractMarkdownFromJsonl = (text: string): string => {
	const lines = text.trim().split("\n");
	const parts: string[] = [];
	for (const line of lines) {
		if (!line.trim()) {
			continue;
		}
		try {
			const obj = JSON.parse(line);
			const results = obj?.result?.layoutParsingResults;
			if (Array.isArray(results)) {
				for (const res of results) {
					const md = res?.markdown?.text;
					if (md) {
						parts.push(md);
					}
				}
			}
		} catch {
			// 忽略无法解析的行
		}
	}
	return parts.join("\n");
};

/**
 * 将 PaddleOCR 的 markdown 结果转成应用内的 OcrDetectResult
 * （PaddleOCR 不返回逐块 box 坐标，这里按行拆分，保证文本内容完整）
 */
const markdownToTextBlocks = (markdown: string): OcrDetectResult => {
	const text = markdown
		.replace(/```[\s\S]*?```/g, (m) => m.replace(/\n/g, " "))
		.split("\n")
		.map((line) => line.trim())
		.filter(Boolean);

	const text_blocks = text.map((line) => ({
		box_points: [
			{ x: 0, y: 0 },
			{ x: 0, y: 0 },
			{ x: 0, y: 0 },
			{ x: 0, y: 0 },
		],
		text: line,
		text_score: 1,
	}));

	return {
		text_blocks,
		scale_factor: 1,
	};
};

/**
 * 调用 PaddlePaddle OCR 云端 API 识别图片文字
 * 流程：上传图片 → 轮询任务状态 → 下载 jsonl 结果 → 转成 OcrDetectResult
 */
export const paddleOcrDetect = async (
	canvas: HTMLCanvasElement,
	settings: PaddleOcrSettings,
): Promise<OcrDetectResult | undefined> => {
	if (!settings.apiUrl || !settings.token) {
		throw new Error("PaddleOCR API 地址或 Token 未配置");
	}

	const blob = await new Promise<Blob | null>((resolve) => {
		canvas.toBlob(resolve, "image/png", 1);
	});
	if (!blob) {
		throw new Error("无法从画布生成图片");
	}

	const formData = new FormData();
	formData.append("model", settings.model || "PaddleOCR-VL-1.6");
	formData.append(
		"optionalPayload",
		JSON.stringify({
			useDocOrientationClassify: false,
			useDocUnwarping: false,
			useChartRecognition: false,
		}),
	);
	formData.append("file", blob, "snowshot_ocr.png");

	const headers: Record<string, string> = {
		Authorization: `bearer ${settings.token}`,
	};

	// 1. 提交任务
	const jobResponse = await fetch(settings.apiUrl, {
		method: "POST",
		headers,
		body: formData,
	});
	if (jobResponse.status !== 200) {
		throw new Error(
			`PaddleOCR 提交失败: ${jobResponse.status} ${jobResponse.statusText}`,
		);
	}
	const jobJson = await jobResponse.json();
	const jobId = jobJson?.data?.jobId;
	if (!jobId) {
		throw new Error("PaddleOCR 未返回 jobId");
	}

	// 2. 轮询任务状态
	const jobUrl = settings.apiUrl.replace(/\/+$/, "") + "/" + jobId;
	let jsonlUrl = "";
	for (let i = 0; i < 60; i++) {
		await sleep(2000);
		const pollResponse = await fetch(jobUrl, {
			headers: { Authorization: `bearer ${settings.token}` },
		});
		if (pollResponse.status !== 200) {
			throw new Error(`PaddleOCR 轮询失败: ${pollResponse.status}`);
		}
		const pollJson = await pollResponse.json();
		const state = pollJson?.data?.state;
		if (state === "done") {
			jsonlUrl = pollJson?.data?.resultUrl?.jsonUrl || "";
			break;
		}
		if (state === "failed") {
			throw new Error(
				`PaddleOCR 任务失败: ${pollJson?.data?.errorMsg || "未知错误"}`,
			);
		}
	}
	if (!jsonlUrl) {
		throw new Error("PaddleOCR 任务超时");
	}

	// 3. 下载结果
	const jsonlResponse = await fetch(jsonlUrl);
	if (jsonlResponse.status !== 200) {
		throw new Error(`PaddleOCR 结果下载失败: ${jsonlResponse.status}`);
	}
	const jsonlText = await jsonlResponse.text();
	const markdown = extractMarkdownFromJsonl(jsonlText);
	return markdownToTextBlocks(markdown);
};

/**
 * 根据应用设置判断当前是否应使用 PaddleOCR 云端引擎
 */
export const shouldUsePaddleOcr = (
	settings: AppSettingsData | undefined,
): boolean => {
	if (!settings) {
		return false;
	}
	const ocrSettings = settings[AppSettingsGroup.FunctionOcr];
	if (!ocrSettings) {
		return false;
	}
	return (
		ocrSettings.enablePaddleOcr === true &&
		ocrSettings.ocrPriority === "paddle" &&
		!!ocrSettings.paddleOcrApiUrl &&
		!!ocrSettings.paddleOcrToken
	);
};

export const getPaddleOcrSettings = (
	settings: AppSettingsData | undefined,
): PaddleOcrSettings | undefined => {
	if (!settings) {
		return undefined;
	}
	const ocrSettings = settings[AppSettingsGroup.FunctionOcr];
	if (!ocrSettings) {
		return undefined;
	}
	return {
		apiUrl: ocrSettings.paddleOcrApiUrl,
		token: ocrSettings.paddleOcrToken,
		model: ocrSettings.paddleOcrModel,
	};
};

export { appError };
