import type {
	DeepLTranslateResult,
	GoogleWebTranslateResult,
	TranslateData,
	TranslateParams,
	TranslationTypeOption,
} from "@/types/servies/translation";
import {
	ServiceResponse,
	type StreamFetchEventOptions,
	serviceBaseFetch,
	serviceFetch,
	streamFetch,
} from ".";

export const translate = async (
	options: StreamFetchEventOptions<TranslateData>,
	params: TranslateParams,
) => {
	return streamFetch<TranslateData>("/api/v1/translation/translate", {
		method: "POST",
		data: params,
		...options,
	});
};

export const getTranslationTypes = async () => {
	return serviceFetch<TranslationTypeOption[]>("/api/v1/translation/types", {
		method: "GET",
	});
};

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

export const translateTextGoogleWeb = async (
	sourceContent: string,
	sourceLanguage: string,
	targetLanguage: string,
): Promise<GoogleWebTranslateResult | undefined> => {
	const response = await serviceBaseFetch(
		`https://translate.google.com/translate_a/single`,
		{
			method: "GET",
			params: {
				client: "gtx",
				dt: "t",
				dj: "1",
				sl: sourceLanguage,
				tl: targetLanguage,
				q: sourceContent,
			},
		},
	);

	if (response instanceof ServiceResponse) {
		response.success();
		return undefined;
	}

	return (await response.json()) as GoogleWebTranslateResult;
};
