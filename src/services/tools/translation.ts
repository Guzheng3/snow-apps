import type {
	TranslateData,
	TranslateParams,
} from "@/types/servies/translation";
import { serviceFetch } from ".";

export const translate = async (params: TranslateParams) => {
	return serviceFetch<TranslateData>("/api/v2/translation/translate", {
		method: "POST",
		data: params,
	});
};