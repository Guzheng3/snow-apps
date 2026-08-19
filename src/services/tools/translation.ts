import { fetch } from "@tauri-apps/plugin-http";
import type {
	DeepLTranslateResult,
	TranslateData,
	TranslateParams,
	TranslationTypeOption,
} from "@/types/servies/translation";
import { withCache } from "@/utils/cache";
import { ServiceResponse, serviceBaseFetch, serviceFetch } from ".";

export const translate = async (params: TranslateParams) => {
	return serviceFetch<TranslateData>("/api/v2/translation/translate", {
		method: "POST",
		data: params,
	});
};

export const getTranslationTypes = async () => {
	return serviceFetch<TranslationTypeOption[]>("/api/v2/translation/types", {
		method: "GET",
	});
};

const fetchTranslationTypes = async (): Promise<
	TranslationTypeOption[] | undefined
> => {
	const resp = await getTranslationTypes();
	if (resp.success()) {
		return resp.data ?? [];
	}
	return undefined;
};

export const getTranslationTypesWithCache = withCache(fetchTranslationTypes, {
	key: "getTranslationTypes",
	duration: 60 * 60 * 1000, // 缓存 1 小时
});

export const translateTextDeepL = async (
	apiUri: string,
	apiKey: string,
	sourceContent: string[],
	sourceLanguage: string | null,
	targetLanguage: string,
	preferQualityOptimized: boolean,
): Promise<DeepLTranslateResult | undefined> => {
	const response = await serviceBaseFetch(apiUri, {
		method: "POST",
		headers: {
			"Content-Type": "application/json",
			Authorization: `DeepL-Auth-Key ${apiKey}`,
		},
		data: {
			text: sourceContent,
			source_lang: sourceLanguage,
			target_lang: targetLanguage,
			preserve_formatting: true,
			model_type: preferQualityOptimized
				? "prefer_quality_optimized"
				: "latency_optimized",
		},
	});

	if (response instanceof ServiceResponse) {
		response.success();
		return undefined;
	}

	return (await response.json()) as DeepLTranslateResult;
};

/**
 * Google 免费翻译接口（无需 API Key，开箱即用）
 * GET https://translate.googleapis.com/translate_a/single?client=gtx&sl=..&tl=..&dt=t&q=..
 */
export const translateTextGoogle = async (
	apiUri: string,
	sourceContent: string[],
	sourceLanguage: string,
	targetLanguage: string,
): Promise<string[] | undefined> => {
	const results: string[] = [];
	for (const text of sourceContent) {
		try {
			const url = new URL(apiUri);
			url.searchParams.set("client", "gtx");
			url.searchParams.set("sl", sourceLanguage || "auto");
			url.searchParams.set("tl", targetLanguage);
			url.searchParams.set("dt", "t");
			url.searchParams.set("q", text);
			const response = await fetch(url.toString(), { method: "GET" });
			if (response.status !== 200) {
				return undefined;
			}
			const json = (await response.json()) as unknown[];
			const segments = json[0] as Array<[string, string]>;
			if (!Array.isArray(segments)) {
				return undefined;
			}
			const translated = segments
				.map((seg) => (Array.isArray(seg) ? seg[0] ?? "" : ""))
				.join("");
			results.push(translated);
		} catch (error) {
			console.error("[translateTextGoogle] error", error);
			return undefined;
		}
	}
	return results;
};

/**
 * 有道智云翻译 v3 签名
 */
const truncate = (q: string): string => {
	const len = q.length;
	return len <= 20 ? q : q.slice(0, 10) + len + q.slice(len - 10);
};

const sha256Hex = async (input: string): Promise<string> => {
	const data = new TextEncoder().encode(input);
	const digest = await crypto.subtle.digest("SHA-256", data);
	return Array.from(new Uint8Array(digest))
		.map((b) => b.toString(16).padStart(2, "0"))
		.join("");
};

/**
 * 有道智云翻译（需 appKey + secretKey，免费注册）
 * POST https://openapi.youdao.com/api，signType=v3
 */
export const translateTextYoudao = async (
	apiUri: string,
	/** appKey:secretKey，用冒号分隔 */
	apiKeyWithSecret: string,
	sourceContent: string[],
	sourceLanguage: string,
	targetLanguage: string,
): Promise<string[] | undefined> => {
	const [appKey, secretKey] = apiKeyWithSecret.split(":");
	if (!appKey || !secretKey) {
		return undefined;
	}
	const results: string[] = [];
	for (const text of sourceContent) {
		try {
			const salt = Date.now().toString();
			const curtime = Math.floor(Date.now() / 1000).toString();
			const sign = await sha256Hex(
				appKey + truncate(text) + salt + curtime + secretKey,
			);
			const body = new URLSearchParams({
				q: text,
				from: sourceLanguage || "auto",
				to: targetLanguage,
				appKey,
				salt,
				sign,
				signType: "v3",
				curtime,
			});
			const response = await fetch(apiUri, {
				method: "POST",
				headers: { "Content-Type": "application/x-www-form-urlencoded" },
				body: body.toString(),
			});
			if (response.status !== 200) {
				return undefined;
			}
			const json = (await response.json()) as {
				translation?: string[];
				errorCode?: string;
			};
			if (!json.translation || json.translation.length === 0) {
				return undefined;
			}
			results.push(json.translation[0]);
		} catch (error) {
			console.error("[translateTextYoudao] error", error);
			return undefined;
		}
	}
	return results;
};
