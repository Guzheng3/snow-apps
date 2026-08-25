export enum TranslationType {
	Youdao = 0,
}

export interface TranslateParams {
	/**
	 * 需要翻译的内容
	 */
	content: string[];
	/**
	 * 源语言
	 */
	from: string;
	/**
	 * 目标语言
	 */
	to: string;
	/**
	 * 翻译类型
	 */
	type: TranslationType;
}

export interface TranslateData {
	/**
	 * 翻译后的内容
	 */
	results: {
		content: string;
	}[];
	/**
	 * 源语言
	 */
	from?: string;
	/**
	 * 目标语言
	 */
	to?: string;
}

export type DeepLTranslateResult = {
	translations: {
		detected_source_language: string;
		text: string;
	}[];
};
