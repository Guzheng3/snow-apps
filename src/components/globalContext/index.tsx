import { App as AntdApp } from "antd";
import type React from "react";
import { useEffect } from "react";
import { HotkeysProvider } from "react-hotkeys-hook";
import "@ant-design/v5-patch-for-react-19";

export const GlobalContext: React.FC<{
	children: React.ReactNode;
}> = ({ children }) => {
	useEffect(() => {
		const handleKeyDown = (event: KeyboardEvent) => {
			if (
				event.key === "F5" ||
				event.key === "F3" ||
				(event.ctrlKey && event.key === "r") ||
				(event.metaKey && event.key === "r") ||
				event.key === "Alt" || // 屏蔽 Alt + A, Alt + A 可能阻塞浏览器??? 逆天 Bug
				// 禁用浏览器前进后退快捷键
				(event.altKey && event.key === "ArrowLeft") || // Alt + 左箭头（后退）
				(event.altKey && event.key === "ArrowRight") // Alt + 右箭头（前进）
			) {
				event.preventDefault();
			}
		};

		document.addEventListener("keydown", handleKeyDown);

		return () => {
			document.removeEventListener("keydown", handleKeyDown);
		};
	}, []);

	return (
		<AntdApp>
			<HotkeysProvider>{children}</HotkeysProvider>
		</AntdApp>
	);
};
