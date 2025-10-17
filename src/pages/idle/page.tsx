"use client";

import { useRouter } from "@tanstack/react-router";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { useCallback, useContext, useEffect } from "react";
import { hotLoadPageAddPage } from "@/commands/hotLoadPage";
import { EventListenerContext } from "@/components/eventListener";
import { useAppSettingsLoad } from "@/hooks/useAppSettingsLoad";

/// 热加载待机页面，当收到启动命令时自动切换到功能页面
export const IdlePage: React.FC = () => {
	const router = useRouter();
	useAppSettingsLoad(
		useCallback(() => {
			hotLoadPageAddPage();
		}, []),
	);

	const { addListener, removeListener } = useContext(EventListenerContext);

	useEffect(() => {
		const listenerId = addListener("hot-load-page-route-push", (args) => {
			const payload = (
				args as {
					payload: {
						label: string;
						url: string;
					};
				}
			).payload;

			if (payload.label !== getCurrentWindow().label) {
				return;
			}

			router.navigate({ to: payload.url });
		});

		return () => {
			removeListener(listenerId);
		};
	}, [addListener, removeListener, router]);

	return undefined;
};
