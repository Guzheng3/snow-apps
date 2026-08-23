import type {
	TranslateData,
	TranslateParams,
	TranslationTypeOption,
} from "@/types/servies/translation";
import { withCache } from "@/utils/cache";
import { ServiceResponse, serviceFetch } from ".";

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
	duration: 60 * 60 * 1000,
});
