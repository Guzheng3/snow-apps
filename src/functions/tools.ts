import { emit } from "@tauri-apps/api/event";

export const updateSelectedText = async () => {
	await emit("update-selected-text");
};

export const executeChat = async () => {
	await emit("execute-chat");
};

export const executeChatSelectedText = async () => {
	await emit("execute-chat-selected-text");
};

export const executeTranslate = async () => {
	await emit("execute-translate");
};

export const executeTranslateSelectedText = async () => {
	await emit("execute-translate-selected-text");
};
