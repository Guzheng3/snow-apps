import { serviceFetch } from ".";

export interface ChatModel {
	model: string;
	name: string;
	thinking: boolean;
	support_vision: boolean;
}

export const getChatModels = async () => {
	return serviceFetch<ChatModel[]>("/api/v1/chat/models", {
		method: "GET",
	});
};
