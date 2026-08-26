import {
	SOURCE_LANGUAGE_ENV_VARIABLE,
	TARGET_LANGUAGE_ENV_VARIABLE,
} from "@/constants/components/translation";

export const getTranslationPrompt = (
	chatPrompt: string,
	sourceLanguage: string,
	targetLanguage: string,
) => {
	return chatPrompt
		.replace(SOURCE_LANGUAGE_ENV_VARIABLE, sourceLanguage)
		.replace(TARGET_LANGUAGE_ENV_VARIABLE, targetLanguage);
};