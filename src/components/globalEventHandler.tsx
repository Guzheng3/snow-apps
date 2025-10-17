import { useRouter } from "@tanstack/react-router";
import React, { useContext, useEffect } from "react";
import { getSelectedText } from "@/commands/core";
import { EventListenerContext } from "@/components/eventListener";
import { encodeParamsValue } from "@/utils/base64";
import { showWindow } from "@/utils/window";

const GlobalEventHandlerCore: React.FC = () => {
	const router = useRouter();

	const { addListener, removeListener } = useContext(EventListenerContext);

	useEffect(() => {
		const listenerIdList: number[] = [];
		listenerIdList.push(
			addListener("execute-chat", () => {
				showWindow();
				router.navigate({ to: `/tools/chat?t=${Date.now()}` });
			}),
			addListener("execute-chat-selected-text", async () => {
				const text = (await getSelectedText()).substring(0, 10000);
				await showWindow();
				router.navigate({
					to: `/tools/chat?selectText=${encodeParamsValue(text)}&t=${Date.now()}`,
				});
			}),
			addListener("execute-translate", () => {
				showWindow();
				router.navigate({ to: `/tools/translation?t=${Date.now()}` });
			}),
			addListener("execute-translate-selected-text", async () => {
				const text = (await getSelectedText()).substring(0, 10000);
				await showWindow();
				router.navigate({
					to: `/tools/translation?selectText=${encodeParamsValue(text)}&t=${Date.now()}`,
				});
			}),
		);

		return () => {
			listenerIdList.forEach((id) => {
				removeListener(id);
			});
		};
	}, [addListener, removeListener, router]);

	return undefined;
};

export const GlobalEventHandler = React.memo(GlobalEventHandlerCore);
