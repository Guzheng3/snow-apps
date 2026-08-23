"use client";

import { Button, Input, Space, Tag, Tooltip, Typography, App, theme } from "antd";
import {
	ArrowUpOutlined,
	ArrowDownOutlined,
	CheckCircleOutlined,
	CloseCircleOutlined,
	LoadingOutlined,
	ApiOutlined,
	KeyOutlined,
	CloudServerOutlined,
} from "@ant-design/icons";
import { useCallback, useEffect, useRef, useState } from "react";
import { cloudTranslateGetEngines, cloudTranslateTest, cloudTranslateSetConfig } from "@/commands/translate";
import { useAppSettingsLoad } from "@/hooks/useAppSettingsLoad";
import { AppSettingsGroup, type AppSettingsData, type CloudTranslationConfig } from "@/types/appSettings";
import { AppSettingsActionContext } from "@/contexts/appSettingsActionContext";
import { useContext } from "react";

const { Text } = Typography;

interface EngineInfo {
	id: string;
	name: string;
	needsKey: boolean;
	isFree: boolean;
}

interface EngineStatus {
	testing: boolean;
	lastResult?: { ok: boolean; text: string };
}

const TEST_TEXT = "Hello world";
const TEST_FROM = "en";
const TEST_TO = "zh";

export const CloudTranslationEngineManager: React.FC = () => {
	const { token } = theme.useToken();
	const { message } = App.useApp();
	const { updateAppSettings } = useContext(AppSettingsActionContext);

	const [engines, setEngines] = useState<EngineInfo[]>([]);
	const [engineOrder, setEngineOrder] = useState<string[]>([]);
	const [config, setConfig] = useState<CloudTranslationConfig>({
		baiduAppId: "",
		baiduAppKey: "",
		bigmodelKey: "",
		engineOrder: [],
	});
	const configRef = useRef(config);
	const engineOrderRef = useRef(engineOrder);
	const [engineStatus, setEngineStatus] = useState<Record<string, EngineStatus>>({});
	const [loading, setLoading] = useState(true);

	// Keep refs in sync
	useEffect(() => { configRef.current = config; }, [config]);
	useEffect(() => { engineOrderRef.current = engineOrder; }, [engineOrder]);

	// Load engines and current config
	useAppSettingsLoad(
		useCallback(
			(settings: AppSettingsData) => {
				const transSettings = settings[AppSettingsGroup.FunctionTranslation];
				const cfg = transSettings.cloudTranslationConfig;
				setConfig(cfg);
				setEngineOrder(cfg.engineOrder);
				setLoading(false);
			},
			[],
		),
		true,
	);

	useEffect(() => {
		cloudTranslateGetEngines()
			.then((list) => setEngines(list as EngineInfo[]))
			.catch(() => {});
	}, []);

	// Flush current config to backend
	const flushConfig = useCallback(async () => {
		const updated = { ...configRef.current, engineOrder: engineOrderRef.current };
		await cloudTranslateSetConfig(updated).catch(() => {
			message.error("保存翻译引擎配置失败");
		});
		updateAppSettings(
			AppSettingsGroup.FunctionTranslation,
			{ cloudTranslationConfig: updated },
			true,
			true,
		);
	}, [updateAppSettings, message]);

	// Move engine up
	const moveUp = useCallback(
		(index: number) => {
			if (index <= 0) return;
			const newOrder = [...engineOrderRef.current];
			[newOrder[index - 1], newOrder[index]] = [newOrder[index], newOrder[index - 1]];
			setEngineOrder(newOrder);
			engineOrderRef.current = newOrder;
			const updated = { ...configRef.current, engineOrder: newOrder };
			cloudTranslateSetConfig(updated).catch(() => {});
			updateAppSettings(
				AppSettingsGroup.FunctionTranslation,
				{ cloudTranslationConfig: updated },
				true,
				true,
			);
		},
		[updateAppSettings],
	);

	// Move engine down
	const moveDown = useCallback(
		(index: number) => {
			if (index >= engineOrderRef.current.length - 1) return;
			const newOrder = [...engineOrderRef.current];
			[newOrder[index], newOrder[index + 1]] = [newOrder[index + 1], newOrder[index]];
			setEngineOrder(newOrder);
			engineOrderRef.current = newOrder;
			const updated = { ...configRef.current, engineOrder: newOrder };
			cloudTranslateSetConfig(updated).catch(() => {});
			updateAppSettings(
				AppSettingsGroup.FunctionTranslation,
				{ cloudTranslationConfig: updated },
				true,
				true,
			);
		},
		[updateAppSettings],
	);

	// Update API key config (local state only, save on blur)
	const updateKeyConfig = useCallback(
		(key: keyof CloudTranslationConfig, value: string) => {
			const newConfig = { ...configRef.current, [key]: value };
			setConfig(newConfig);
			configRef.current = newConfig;
		},
		[],
	);

	// Test a single engine
	const testEngine = useCallback(
		async (engineId: string) => {
			// Save config first to ensure API keys are set
			await flushConfig();

			setEngineStatus((prev) => ({
				...prev,
				[engineId]: { testing: true },
			}));

			try {
				const result = await cloudTranslateTest(engineId, TEST_TEXT, TEST_FROM, TEST_TO);
				setEngineStatus((prev) => ({
					...prev,
					[engineId]: { testing: false, lastResult: { ok: true, text: result } },
				}));
				message.success(`${getEngineName(engineId)} 测试成功`);
			} catch (error) {
				const errMsg = String(error);
				setEngineStatus((prev) => ({
					...prev,
					[engineId]: { testing: false, lastResult: { ok: false, text: errMsg } },
				}));
				message.error(`${getEngineName(engineId)} 测试失败: ${errMsg}`);
			}
		},
		[flushConfig, message],
	);

	const getEngineName = (id: string) => engines.find((e) => e.id === id)?.name ?? id;

	const getEngineInfo = (id: string) => engines.find((e) => e.id === id);

	if (loading) {
		return (
			<div style={{ padding: token.paddingMD, color: token.colorTextSecondary }}>
				加载中...
			</div>
		);
	}

	return (
		<div style={{ padding: `${token.paddingSM}px 0` }}>
			<Text type="secondary" style={{ fontSize: 12, marginBottom: token.marginSM, display: "block" }}>
				引擎按优先级从上到下排列，排第一的作为默认源。翻译时按顺序自动 fallback。
			</Text>

			<div
				style={{
					border: `1px solid ${token.colorBorderSecondary}`,
					borderRadius: token.borderRadiusLG,
					overflow: "hidden",
				}}
			>
				{engineOrder.map((engineId, index) => {
					const info = getEngineInfo(engineId);
					const status = engineStatus[engineId];

					return (
						<div
							key={engineId}
							style={{
								display: "flex",
								alignItems: "flex-start",
								padding: `${token.paddingSM}px ${token.paddingMD}px`,
								borderBottom:
									index < engineOrder.length - 1
										? `1px solid ${token.colorBorderSecondary}`
										: "none",
								background: index === 0 ? token.colorBgTextHover : undefined,
								gap: token.marginSM,
							}}
						>
							{/* Priority badge */}
							<div
								style={{
									minWidth: 28,
									textAlign: "center",
									paddingTop: 4,
								}}
							>
								<Tag
									color={index === 0 ? "blue" : "default"}
									style={{ margin: 0 }}
								>
									{index === 0 ? "默认" : `#${index + 1}`}
								</Tag>
							</div>

							{/* Engine info */}
							<div style={{ flex: 1, minWidth: 0 }}>
								<div style={{ display: "flex", alignItems: "center", gap: token.marginXS, marginBottom: 4 }}>
									<CloudServerOutlined style={{ color: token.colorPrimary }} />
									<Text strong>{info?.name ?? engineId}</Text>
									<Tag
										color={info?.isFree ? "green" : "orange"}
										style={{ fontSize: 11, lineHeight: "18px" }}
									>
										{info?.isFree ? "免费" : "需Key"}
									</Tag>
									<Tag style={{ fontSize: 11, lineHeight: "18px" }}>{engineId}</Tag>
								</div>

								{/* API Key inputs for engines that need them */}
								{info?.needsKey && engineId === "baidu" && (
									<div style={{ display: "flex", gap: token.marginSM, marginTop: token.marginXS }}>
										<Input
											size="small"
											placeholder="百度 AppID"
											prefix={<KeyOutlined />}
											defaultValue={config.baiduAppId}
											onChange={(e) => updateKeyConfig("baiduAppId", e.target.value)}
											onBlur={() => flushConfig()}
											style={{ maxWidth: 200 }}
										/>
										<Input.Password
											size="small"
											placeholder="百度 AppKey"
											prefix={<KeyOutlined />}
											defaultValue={config.baiduAppKey}
											onChange={(e) => updateKeyConfig("baiduAppKey", e.target.value)}
											onBlur={() => flushConfig()}
											style={{ maxWidth: 260 }}
										/>
									</div>
								)}
								{info?.needsKey && engineId === "bigmodel" && (
									<div style={{ marginTop: token.marginXS }}>
										<Input.Password
											size="small"
											placeholder="智谱 GLM API Key"
											prefix={<KeyOutlined />}
											defaultValue={config.bigmodelKey}
											onChange={(e) => updateKeyConfig("bigmodelKey", e.target.value)}
											onBlur={() => flushConfig()}
											style={{ maxWidth: 360 }}
										/>
									</div>
								)}

								{/* Test result */}
								{status?.lastResult && !status.testing && (
									<div
										style={{
											marginTop: 4,
											fontSize: 12,
											color: status.lastResult.ok
												? token.colorSuccess
												: token.colorError,
										}}
									>
										{status.lastResult.ok ? (
											<Space size={4}>
												<CheckCircleOutlined />
												<Text
													ellipsis
													style={{ fontSize: 12, maxWidth: 300 }}
												>
													"{TEST_TEXT}" → "{status.lastResult.text}"
												</Text>
											</Space>
										) : (
											<Space size={4}>
												<CloseCircleOutlined />
												<Text
													ellipsis
													style={{ fontSize: 12, maxWidth: 300, color: token.colorError }}
												>
													{status.lastResult.text}
												</Text>
											</Space>
										)}
									</div>
								)}
							</div>

							{/* Action buttons */}
							<Space size={4} style={{ flexShrink: 0, paddingTop: 2 }}>
								{/* Reorder buttons */}
								<Tooltip title="上移（提高优先级）">
									<Button
										size="small"
										type="text"
										icon={<ArrowUpOutlined />}
										disabled={index === 0}
										onClick={() => moveUp(index)}
									/>
								</Tooltip>
								<Tooltip title="下移（降低优先级）">
									<Button
										size="small"
										type="text"
										icon={<ArrowDownOutlined />}
										disabled={index === engineOrder.length - 1}
										onClick={() => moveDown(index)}
									/>
								</Tooltip>

								{/* Test button */}
								<Tooltip title="测试此引擎">
									<Button
										size="small"
										type="default"
										icon={
											status?.testing ? (
												<LoadingOutlined />
											) : (
												<ApiOutlined />
											)
										}
										loading={status?.testing}
										onClick={() => testEngine(engineId)}
									>
										测试
									</Button>
								</Tooltip>
							</Space>
						</div>
					);
				})}
			</div>
		</div>
	);
};