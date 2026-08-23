"use client";

import React from "react";
import ProForm, {
	ProFormDependency,
	ProFormDigit,
	ProFormList,
	ProFormSelect,
	ProFormSwitch,
	ProFormText,
	ProFormTextArea,
} from "@ant-design/pro-form";
import {
	Alert,
	App,
	Button,
	Col,
	ColorPicker,
	Divider,
	Flex,
	Form,
	Input,
	List,
	Modal,
	Row,
	Select,
	type SelectProps,
	Spin,
	Switch,
	Tabs,
	Tag,
	Tooltip,
	Typography,
	theme,
} from "antd";
import type { AggregationColor } from "antd/es/color-picker/color";
import {
	useCallback,
	useContext,
	useEffect,
	useMemo,
	useRef,
	useState,
} from "react";
import { FormattedMessage, useIntl } from "react-intl";
import { videoRecordGetMicrophoneDeviceNames } from "@/commands/videoRecord";
import { ContentWrap } from "@/components/contentWrap";
import { DirectoryInput } from "@/components/directoryInput";
import { GroupTitle, SubGroupTitle } from "@/components/groupTitle";
import { IconLabel } from "@/components/iconLable";
import { ResetSettingsButton } from "@/components/resetSettingsButton";
import { defaultAppSettingsData } from "@/constants/appSettings";
import { FOCUS_WINDOW_APP_NAME_ENV_VARIABLE } from "@/constants/components/chat";
import {
	SOURCE_LANGUAGE_ENV_VARIABLE,
	TARGET_LANGUAGE_ENV_VARIABLE,
	TRANSLATION_DOMAIN_ENV_VARIABLE,
} from "@/constants/components/translation";
import {
	PLUGIN_ID_AI_CHAT,
	PLUGIN_ID_FFMPEG,
	PLUGIN_ID_RAPID_OCR,
	PLUGIN_ID_TRANSLATE,
} from "@/constants/pluginService";
import { AppSettingsActionContext } from "@/contexts/appSettingsActionContext";
import { usePluginServiceContext } from "@/contexts/pluginServiceContext";
import { useAppSettingsLoad } from "@/hooks/useAppSettingsLoad";
import { usePlatform } from "@/hooks/usePlatform";
import { useVisionModelList } from "@/pages/fixedContent/components/ocrResult";
import {
	type AppSettingsData,
	AppSettingsFixedContentInitialPosition,
	AppSettingsGroup,
	CloudSaveUrlFormat,
	CloudSaveUrlType,
	type CloudTranslationConfig,
	DoubleClickAction,
	GifFormat,
	KeyDisplayDirection,
	OcrDetectAfterAction,
	OcrModel,
	TranslationApiType,
	TrayIconClickAction,
	VideoMaxSize,
} from "@/types/appSettings";
import { DrawState } from "@/types/draw";
import { ImageFormat } from "@/types/utils/file";
import {
	generateImageFileName,
	getImageSaveDirectory,
	getVideoRecordSaveDirectory,
} from "@/utils/file";
import { TestChat } from "./components/testChat";
import { TranslationConfig } from "./components/translationConfig";

/** 云翻译引擎元数据 */
const CLOUD_ENGINE_META: Record<
	TranslationApiType,
	{ label: string; needKey: boolean; tag: string; desc: string }
> = {
	[TranslationApiType.Transmart]: {
		label: "腾讯通天塔",
		needKey: false,
		tag: "green",
		desc: "免 Key · QQ 翻译接口",
	},
	[TranslationApiType.ICiba]: {
		label: "金山词霸",
		needKey: false,
		tag: "green",
		desc: "免 Key · ICiba 批量翻译接口",
	},
	[TranslationApiType.Yandex]: {
		label: "Yandex",
		needKey: false,
		tag: "green",
		desc: "免 Key · 模拟安卓客户端",
	},
	[TranslationApiType.Baidu]: {
		label: "百度翻译",
		needKey: true,
		tag: "orange",
		desc: "需 AppID + AppKey，接口地址已内置",
	},
	[TranslationApiType.BigModel]: {
		label: "智谱 GLM",
		needKey: true,
		tag: "orange",
		desc: "需 API Key，接口地址已内置",
	},
	[TranslationApiType.DeepL]: {
		label: "DeepL",
		needKey: true,
		tag: "orange",
		desc: "DeepL 翻译",
	},
	[TranslationApiType.Google]: {
		label: "Google",
		needKey: false,
		tag: "green",
		desc: "Google 网页翻译",
	},
	[TranslationApiType.Youdao]: {
		label: "有道翻译",
		needKey: false,
		tag: "green",
		desc: "有道翻译",
	},
};

export const FunctionSettingsPage = () => {
	const intl = useIntl();
	const { token } = theme.useToken();

	const { updateAppSettings } = useContext(AppSettingsActionContext);
	const [functionForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionChat]>();
	const [functionDrawForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionDraw]>();
	const [trayIconForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionTrayIcon]>();
	const [translationForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionTranslation]>();
	const [screenshotForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionScreenshot]>();
	const [outputForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionOutput]>();
	const [fullScreenDrawForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionFullScreenDraw]>();
	const [fixedContentForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionFixedContent]>();
	const [videoRecordForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionVideoRecord]>();
	const [functionOcrForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionOcr]>();
	const [paddlePasteOpen, setPaddlePasteOpen] = useState(false);
	const [paddlePasteText, setPaddlePasteText] = useState("");
	const [paddleParseTip, setPaddleParseTip] = useState<React.ReactNode>(null);
	const [functionGlobalShortcutForm] =
		Form.useForm<AppSettingsData[AppSettingsGroup.FunctionGlobalShortcut]>();

	const [appSettingsLoading, setAppSettingsLoading] = useState(true);

	// 功能设置压栈切换：当前激活的分组 tab
	const [activeTab, setActiveTab] = useState("screenshotSettings");

	// 云翻译引擎配置（百度/智谱 Key + 引擎优先级）
	const [translationCloudConfig, setTranslationCloudConfig] = useState<
		CloudTranslationConfig
	>({
		baiduAppId: "",
		baiduAppKey: "",
		bigmodelKey: "",
		engineOrder: [
			TranslationApiType.Transmart,
			TranslationApiType.ICiba,
			TranslationApiType.Yandex,
			TranslationApiType.Baidu,
			TranslationApiType.BigModel,
		],
	});
	// 当前展开配置面板的引擎
	const [cloudEngineConfigOpen, setCloudEngineConfigOpen] = useState<
		TranslationApiType | undefined
	>(undefined);
	// 连通性测试状态
	const [cloudEngineTesting, setCloudEngineTesting] = useState<
		TranslationApiType | undefined
	>(undefined);
	const cloudEngineOrder = translationCloudConfig.engineOrder;
	const { message } = App.useApp();

	// 云翻译引擎连通性测试（模拟）
	const cloudEngineTest = useCallback(
		(engine: TranslationApiType) => {
			const meta = CLOUD_ENGINE_META[engine];
			let hasKey = false;
			if (engine === TranslationApiType.Baidu) {
				hasKey =
					!!translationCloudConfig.baiduAppId &&
					!!translationCloudConfig.baiduAppKey;
			} else if (engine === TranslationApiType.BigModel) {
				hasKey = !!translationCloudConfig.bigmodelKey;
			}
			if (!hasKey) {
				message.warning("请先填写 Key 后再测试连通性");
				return;
			}
			setCloudEngineTesting(engine);
			setTimeout(() => {
				setCloudEngineTesting(undefined);
				// 演示环境模拟成功
				message.success(`${meta.label} 连通性测试通过`);
			}, 1200);
		},
		[message, translationCloudConfig],
	);

	useAppSettingsLoad(
		useCallback(
			(settings: AppSettingsData, preSettings?: AppSettingsData) => {
				setAppSettingsLoading(false);

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionTranslation] !==
						settings[AppSettingsGroup.FunctionTranslation]
				) {
					translationForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionTranslation],
					);
					if (settings[AppSettingsGroup.FunctionTranslation].cloudTranslationConfig) {
						setTranslationCloudConfig(
							settings[AppSettingsGroup.FunctionTranslation]
								.cloudTranslationConfig,
						);
					}
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionChat] !==
						settings[AppSettingsGroup.FunctionChat]
				) {
					functionForm.setFieldsValue(settings[AppSettingsGroup.FunctionChat]);
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionDraw] !==
						settings[AppSettingsGroup.FunctionDraw]
				) {
					functionDrawForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionDraw],
					);
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionScreenshot] !==
						settings[AppSettingsGroup.FunctionScreenshot]
				) {
					screenshotForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionScreenshot],
					);

					const screenshotSettings =
						settings[AppSettingsGroup.FunctionScreenshot];
					if (!screenshotSettings.saveFileDirectory) {
						getImageSaveDirectory(settings).then((saveDirectory) => {
							screenshotSettings.saveFileDirectory = saveDirectory;
							screenshotForm.setFieldsValue(screenshotSettings);
						});
					}
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionOutput] !==
						settings[AppSettingsGroup.FunctionOutput]
				) {
					outputForm.setFieldsValue(settings[AppSettingsGroup.FunctionOutput]);
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionFixedContent] !==
						settings[AppSettingsGroup.FunctionFixedContent]
				) {
					fixedContentForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionFixedContent],
					);
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionFullScreenDraw] !==
						settings[AppSettingsGroup.FunctionFullScreenDraw]
				) {
					fullScreenDrawForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionFullScreenDraw],
					);
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionVideoRecord] !==
						settings[AppSettingsGroup.FunctionVideoRecord]
				) {
					const videoRecordSettings =
						settings[AppSettingsGroup.FunctionVideoRecord];
					videoRecordForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionVideoRecord],
					);
					if (!videoRecordSettings.saveDirectory) {
						getVideoRecordSaveDirectory(settings).then((saveDirectory) => {
							videoRecordSettings.saveDirectory = saveDirectory;
							videoRecordForm.setFieldsValue(videoRecordSettings);
						});
					}
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionTrayIcon] !==
						settings[AppSettingsGroup.FunctionTrayIcon]
				) {
					trayIconForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionTrayIcon],
					);
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionOcr] !==
						settings[AppSettingsGroup.FunctionOcr]
				) {
					functionOcrForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionOcr],
					);
				}

				if (
					preSettings === undefined ||
					preSettings[AppSettingsGroup.FunctionGlobalShortcut] !==
						settings[AppSettingsGroup.FunctionGlobalShortcut]
				) {
					functionGlobalShortcutForm.setFieldsValue(
						settings[AppSettingsGroup.FunctionGlobalShortcut],
					);
				}
			},
			[
				translationForm,
				functionForm,
				functionDrawForm,
				screenshotForm,
				outputForm,
				fixedContentForm,
				fullScreenDrawForm,
				videoRecordForm,
				trayIconForm,
				functionOcrForm,
				functionGlobalShortcutForm,
			],
		),
		true,
	);

	const [microphoneDeviceNameOptions, setMicrophoneDeviceNameOptions] =
		useState<{ label: string; value: string }[]>([]);

	const [currentPlatform] = usePlatform();

	const formatMicrophoneDeviceName = useCallback(
		(microphoneDeviceName: string) => {
			if (currentPlatform !== "macos") {
				return microphoneDeviceName;
			}

			// 匹配格式: [0] 设备名，直接提取设备名部分
			const regex = /\[\d+\]\s+(.+)/;
			const match = microphoneDeviceName.match(regex);

			if (match?.[1]) {
				return match[1].trim();
			}

			return microphoneDeviceName;
		},
		[currentPlatform],
	);

	const { isReadyStatus } = usePluginServiceContext();

	const initedMicrophoneDeviceNameOptions = useRef(false);
	useEffect(() => {
		if (initedMicrophoneDeviceNameOptions.current) {
			return;
		}

		if (!isReadyStatus?.(PLUGIN_ID_FFMPEG)) {
			return;
		}

		initedMicrophoneDeviceNameOptions.current = true;

		const options: { label: string; value: string }[] = [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.microphoneDeviceName.default",
				}),
				value: "",
			},
		];

		videoRecordGetMicrophoneDeviceNames()
			.then((microphoneDeviceNames) => {
				for (const microphoneDeviceName of microphoneDeviceNames) {
					options.push({
						label: formatMicrophoneDeviceName(microphoneDeviceName),
						value: microphoneDeviceName,
					});
				}
			})
			.finally(() => {
				setMicrophoneDeviceNameOptions(options);
			});
	}, [formatMicrophoneDeviceName, intl, isReadyStatus]);

	const videoMaxSizeOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p2160",
				}),
				value: VideoMaxSize.P2160,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p1440",
				}),
				value: VideoMaxSize.P1440,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p1080",
				}),
				value: VideoMaxSize.P1080,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p720",
				}),
				value: VideoMaxSize.P720,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p480",
				}),
				value: VideoMaxSize.P480,
			},
		];
	}, [intl]);

	const gifMaxSizeOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p1080",
				}),
				value: VideoMaxSize.P1080,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p720",
				}),
				value: VideoMaxSize.P720,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.videoMaxSize.p480",
				}),
				value: VideoMaxSize.P480,
			},
		];
	}, [intl]);

	const gifFormatOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.gifFormat.gif",
				}),

				value: GifFormat.Gif,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.gifFormat.apng",
				}),

				value: GifFormat.Apng,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.gifFormat.webp",
				}),

				value: GifFormat.Webp,
			},
		];
	}, [intl]);

	const trayIconClickActionOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.trayIconSettings.iconClickAction.screenshot",
				}),
				value: TrayIconClickAction.Screenshot,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.trayIconSettings.iconClickAction.showMainWindow",
				}),
				value: TrayIconClickAction.ShowMainWindow,
			},
		];
	}, [intl]);

	const disableQuickSelectElementToolListOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "draw.rectTool",
				}),
				value: DrawState.Rect,
			},
			{
				label: intl.formatMessage({
					id: "draw.diamondTool",
				}),
				value: DrawState.Diamond,
			},
			{
				label: intl.formatMessage({
					id: "draw.ellipseTool",
				}),
				value: DrawState.Ellipse,
			},
			{
				label: intl.formatMessage({
					id: "draw.arrowTool",
				}),
				value: DrawState.Arrow,
			},
			{
				label: intl.formatMessage({
					id: "draw.lineTool",
				}),
				value: DrawState.Line,
			},
			{
				label: intl.formatMessage({
					id: "draw.penTool",
				}),
				value: DrawState.Pen,
			},
			{
				label: intl.formatMessage({
					id: "draw.serialNumberTool",
				}),
				value: DrawState.SerialNumber,
			},
			{
				label: intl.formatMessage({
					id: "draw.blurTool",
				}),
				value: DrawState.Blur,
			},
		];
	}, [intl]);

	const ocrAfterActionOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.screenshotSettings.ocrAfterAction.none",
				}),
				value: OcrDetectAfterAction.None,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.screenshotSettings.ocrAfterAction.copyText",
				}),
				value: OcrDetectAfterAction.CopyText,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.screenshotSettings.ocrAfterAction.copyTextAndCloseWindow",
				}),
				value: OcrDetectAfterAction.CopyTextAndCloseWindow,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.screenshotSettings.ocrAfterAction.ocrDetectCopyText",
				}),
				value: OcrDetectAfterAction.OcrDetectCopyText,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.screenshotSettings.ocrAfterAction.ocrDetectCopyTextAndCloseWindow",
				}),
				value: OcrDetectAfterAction.OcrDetectCopyTextAndCloseWindow,
			},
		];
	}, [intl]);

	const initialPositionOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.fixedContentSettings.initialPosition.monitorCenter",
				}),
				value: AppSettingsFixedContentInitialPosition.MonitorCenter,
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.fixedContentSettings.initialPosition.mousePosition",
				}),
				value: AppSettingsFixedContentInitialPosition.MousePosition,
			},
		];
	}, [intl]);

	const fullScreenDrawDefaultToolOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "draw.selectTool",
				}),
				value: DrawState.Select,
			},
			{
				label: intl.formatMessage({
					id: "draw.penTool",
				}),
				value: DrawState.Pen,
			},
			{
				label: intl.formatMessage({
					id: "draw.laserPointerTool",
				}),
				value: DrawState.LaserPointer,
			},
		];
	}, [intl]);

	const translationApiTypeOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.translationSettings.apiConfig.apiType.deepL",
				}),
				value: TranslationApiType.DeepL,
			},
			{
				label: "Google 翻译（免费，无需 Key）",
				value: TranslationApiType.Google,
			},
			{
				label: "有道智云翻译（免费注册，Key 填 appKey:secretKey）",
				value: TranslationApiType.Youdao,
			},
		];
	}, [intl]);

	const encoderPresetOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.encoderPreset.ultrafast",
				}),
				value: "ultrafast",
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.encoderPreset.veryfast",
				}),
				value: "veryfast",
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.encoderPreset.medium",
				}),
				value: "medium",
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.encoderPreset.slower",
				}),
				value: "slower",
			},
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.videoRecordSettings.encoderPreset.placebo",
				}),
				value: "placebo",
			},
		];
	}, [intl]);

	const cloudSaveUrlTypeOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.functionSettings.screenshotSettings.cloudSaveUrl.type.s3",
				}),
				value: CloudSaveUrlType.S3,
			},
		];
	}, [intl]);

	const cloudSaveUrlFormatOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({ id: "draw.cloudSaveUrlFormat.origin" }),
				value: CloudSaveUrlFormat.Origin,
			},
			{
				label: intl.formatMessage({ id: "draw.cloudSaveUrlFormat.markdown" }),
				value: CloudSaveUrlFormat.Markdown,
			},
		];
	}, [intl]);

	const ocrModelOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({
					id: "settings.systemSettings.screenshotSettings.ocrModel.rapidOcrV4",
				}),
				value: OcrModel.RapidOcrV4,
			},
			{
				label: intl.formatMessage({
					id: "settings.systemSettings.screenshotSettings.ocrModel.rapidOcrV5",
				}),
				value: OcrModel.RapidOcrV5,
			},
		];
	}, [intl]);

	const { getVisionModelList } = useVisionModelList();
	const [htmlVisionModelOptions, setHtmlVisionModelOptions] = useState<
		SelectProps["options"]
	>([]);
	useEffect(() => {
		getVisionModelList().then((visionModelList) => {
			const officialVisionModelList = visionModelList.filter(
				(model) => model.isOfficial,
			);
			const customVisionModelList = visionModelList.filter(
				(model) => !model.isOfficial,
			);

			const htmlVisionModelOptions = [
				{
					label: (
						<IconLabel
							label={intl.formatMessage({
								id: "settings.functionSettings.ocrSettings.htmlVisionModel.default",
							})}
							tooltipTitle={intl.formatMessage({
								id: "settings.functionSettings.ocrSettings.htmlVisionModel.default.tip",
							})}
						/>
					),
					value:
						defaultAppSettingsData[AppSettingsGroup.FunctionOcr]
							.htmlVisionModel,
				},
				customVisionModelList.length > 0
					? {
							label: <FormattedMessage id="tools.chat.custom" />,
							options: customVisionModelList.map((model) => ({
								label: model.config.model_name,
								value: model.config.model_name,
							})),
						}
					: undefined,
				officialVisionModelList.length > 0
					? {
							label: <FormattedMessage id="tools.chat.official" />,
							options: officialVisionModelList.map((model) => ({
								label: model.config.model_name,
								value: model.config.model_name,
							})),
						}
					: undefined,
			].filter(Boolean) as SelectProps["options"];

			setHtmlVisionModelOptions(htmlVisionModelOptions);
		});
	}, [getVisionModelList, intl]);

	const doubleClickActionOptions = useMemo(() => {
		return [
			{
				label: intl.formatMessage({ id: "draw.doubleClickAction.copy" }),
				value: DoubleClickAction.Copy,
			},
			{
				label: intl.formatMessage({ id: "draw.doubleClickAction.save" }),
				value: DoubleClickAction.Save,
			},
			{
				label: intl.formatMessage({
					id: "draw.doubleClickAction.fixedToScreen",
				}),
				value: DoubleClickAction.FixedToScreen,
			},
			{
				label: intl.formatMessage({ id: "draw.doubleClickAction.none" }),
				value: DoubleClickAction.None,
			},
		];
	}, [intl]);

	return (
		<ContentWrap>
			{/* 功能设置压栈切换导航 */}
			<Tabs
				activeKey={activeTab}
				onChange={setActiveTab}
				size="small"
				style={{ marginBottom: 12 }}
				items={[
					{ key: "screenshotSettings", label: intl.formatMessage({ id: "settings.functionSettings.screenshotSettings" }) },
					{ key: "functionDrawSettings", label: intl.formatMessage({ id: "settings.functionSettings.drawSettings" }) },
					{ key: "fixedContentSettings", label: intl.formatMessage({ id: "settings.functionSettings.fixedContentSettings" }) },
					{ key: "ocrSettings", label: intl.formatMessage({ id: "settings.functionSettings.ocrSettings" }) },
					{ key: "translationSettings", label: intl.formatMessage({ id: "settings.functionSettings.translationSettings" }) },
					{ key: "chatSettings", label: intl.formatMessage({ id: "settings.functionSettings.chatSettings" }) },
					{ key: "fullScreenDrawSettings", label: intl.formatMessage({ id: "settings.functionSettings.fullScreenDrawSettings" }) },
					{ key: "videoRecordSettings", label: intl.formatMessage({ id: "settings.functionSettings.videoRecordSettings" }) },
					{ key: "trayIconSettings", label: intl.formatMessage({ id: "settings.commonSettings.trayIconSettings" }) },
					{ key: "globalShortcutSettings", label: intl.formatMessage({ id: "settings.functionSettings.globalShortcutSettings" }) },
					{ key: "outputSettings", label: intl.formatMessage({ id: "settings.functionSettings.outputSettings" }) },
				]}
			/>
			<div style={{ display: activeTab === "screenshotSettings" ? undefined : "none" }}>
			<GroupTitle
				id="screenshotSettings"
				extra={
					<ResetSettingsButton
						title={
							<FormattedMessage id="settings.functionSettings.screenshotSettings" />
						}
						appSettingsGroup={AppSettingsGroup.FunctionScreenshot}
					/>
				}
			>
				<FormattedMessage id="settings.functionSettings.screenshotSettings" />
			</GroupTitle>

			<Spin spinning={appSettingsLoading}>
				<ProForm
					form={screenshotForm}
					onValuesChange={(_, values) => {
						updateAppSettings(
							AppSettingsGroup.FunctionScreenshot,
							values,
							true,
							true,
							true,
							true,
							false,
						);
					}}
					submitter={false}
					layout="horizontal"
				>
					<Row gutter={token.marginLG}>
						{currentPlatform !== "macos" && (
							<Col span={12}>
								<ProFormSwitch
									name="findChildrenElements"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.screenshotSettings.findChildrenElements" />
									}
								/>
							</Col>
						)}

						<Col span={12}>
							<ProFormSwitch
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.shortcutCanleTip" />
										}
										tooltipTitle={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.shortcutCanleTip.tip" />
										}
									/>
								}
								name="shortcutCanleTip"
								layout="horizontal"
							/>
						</Col>
					</Row>

					{currentPlatform === "windows" && (
						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSwitch
									name="enableSTranslate"
									layout="horizontal"
									label={
										<IconLabel
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.sTranslate.enable" />
											}
											tooltipTitle={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.sTranslate.enable.tip" />
											}
										/>
									}
								/>
							</Col>
							<Col span={12}>
								<ProFormDependency name={["enableSTranslate"]}>
									{({ enableSTranslate }) => (
										<ProFormDigit
											name="sTranslatePort"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.sTranslate.port" />
											}
											min={1}
											max={65535}
											disabled={!enableSTranslate}
										/>
									)}
								</ProFormDependency>
							</Col>
						</Row>
					)}

					{isReadyStatus?.(PLUGIN_ID_RAPID_OCR) && (
						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSelect
									name="ocrAfterAction"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.screenshotSettings.ocrAfterAction" />
									}
									options={ocrAfterActionOptions}
								/>
							</Col>

							<Col span={12}>
								<ProFormSwitch
									name="ocrCopyText"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.screenshotSettings.ocrCopyText" />
									}
								/>
							</Col>
						</Row>
					)}

					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSelect
								name="doubleClickAction"
								layout="horizontal"
								label={
									<IconLabel
										label={<FormattedMessage id="draw.doubleClickAction" />}
									/>
								}
								options={doubleClickActionOptions}
							/>
						</Col>
					</Row>

					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSwitch
								name="focusedWindowCopyToClipboard"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.screenshotSettings.focusedWindowCopyToClipboard" />
								}
							/>
						</Col>

						<Col span={12}>
							<ProFormSwitch
								name="fullScreenCopyToClipboard"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.screenshotSettings.fullScreenCopyToClipboard" />
								}
							/>
						</Col>
					</Row>

					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSwitch
								name="copyImageFileToClipboard"
								layout="horizontal"
								label={
									<IconLabel
										label={
											<FormattedMessage id="draw.copyImageFileToClipboard" />
										}
										tooltipTitle={
											<FormattedMessage id="draw.copyImageFileToClipboard.tip" />
										}
									/>
								}
							/>
						</Col>

						<Col span={12}>
							<ProFormSwitch
								name="autoSaveOnCopy"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.screenshotSettings.autoSaveFileMode.autoSave" />
								}
							/>
						</Col>

						<Col span={12}>
							<ProFormSwitch
								name="fastSave"
								layout="horizontal"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.autoSaveFileMode.fastSave" />
										}
										tooltipTitle={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.autoSaveFileMode.fastSave.tip" />
										}
									/>
								}
							/>
						</Col>
					</Row>

					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProForm.Item
								name="saveFileDirectory"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.autoSaveFileMode.directory" />
										}
									/>
								}
								required={false}
							>
								<DirectoryInput />
							</ProForm.Item>
						</Col>

						<Col span={12}>
							<ProForm.Item
								name="saveFileFormat"
								label={
									<FormattedMessage id="settings.functionSettings.screenshotSettings.autoSaveFileMode.saveFileFormat" />
								}
							>
								<Select
									options={[
										{
											label: "PNG(*.png)",
											value: ImageFormat.PNG,
										},
										{
											label: "JPEG(*.jpg)",
											value: ImageFormat.JPEG,
										},
										{
											label: "WEBP(*.webp)",
											value: ImageFormat.WEBP,
										},
										{
											label: "AVIF(*.avif)",
											value: ImageFormat.AVIF,
										},
										{
											label: "JPEG XL(*.jxl)",
											value: ImageFormat.JPEG_XL,
										},
									]}
								/>
							</ProForm.Item>
						</Col>
					</Row>

					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSwitch
								name="saveToCloud"
								layout="horizontal"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.saveToCloud" />
										}
										tooltipTitle={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.saveToCloud.tip" />
										}
									/>
								}
								valuePropName="checked"
							/>
						</Col>
					</Row>

					<ProFormDependency<{ saveToCloud: boolean }> name={["saveToCloud"]}>
						{({ saveToCloud }) => {
							if (!saveToCloud) {
								return null;
							}

							return (
								<Row gutter={token.marginLG}>
									<Col span={12}>
										<ProFormSelect
											name="cloudSaveUrlFormat"
											layout="horizontal"
											label={<FormattedMessage id="draw.cloudSaveUrlFormat" />}
											options={cloudSaveUrlFormatOptions}
										/>
									</Col>
									<Col span={12}>
										<ProFormText
											name="cloudProxyUrl"
											layout="horizontal"
											label={
												<IconLabel
													label={
														<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudProxyUrl" />
													}
													tooltipTitle={
														<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudProxyUrl.tip" />
													}
												/>
											}
										/>
									</Col>
									<Col span={12}>
										<ProFormSelect
											name="cloudSaveUrlType"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.type" />
											}
											options={cloudSaveUrlTypeOptions}
										/>
									</Col>
									<Col span={12}>
										<ProFormText
											name="s3Endpoint"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.s3Endpoint" />
											}
										/>
									</Col>
									<Col span={12}>
										<ProFormText.Password
											name="s3AccessKeyId"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.s3AccessKeyId" />
											}
										/>
									</Col>
									<Col span={12}>
										<ProFormText.Password
											name="s3SecretAccessKey"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.s3SecretAccessKey" />
											}
										/>
									</Col>
									<Col span={12}>
										<ProFormText
											name="s3Region"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.s3Region" />
											}
										/>
									</Col>
									<Col span={12}>
										<ProFormText
											name="s3BucketName"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.s3BucketName" />
											}
										/>
									</Col>
									<Col span={12}>
										<ProFormText
											name="s3PathPrefix"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.s3PathPrefix" />
											}
										/>
									</Col>
									<Col span={12}>
										<ProFormSwitch
											name="s3ForcePathStyle"
											layout="horizontal"
											label={
												<FormattedMessage id="settings.functionSettings.screenshotSettings.cloudSaveUrl.s3ForcePathStyle" />
											}
										/>
									</Col>
								</Row>
							);
						}}
					</ProFormDependency>
				</ProForm>
			</Spin>
			</div>

			<div style={{ display: activeTab === "functionDrawSettings" ? undefined : "none" }}>
			<Divider />

			<GroupTitle
				id="functionDrawSettings"
				extra={
					<ResetSettingsButton
						title={intl.formatMessage({ id: "settings.commonSettings.draw" })}
						appSettingsGroup={AppSettingsGroup.FunctionDraw}
					/>
				}
			>
				<FormattedMessage id="settings.commonSettings.draw" />
			</GroupTitle>

			<ProForm<AppSettingsData[AppSettingsGroup.FunctionDraw]>
				className="settings-form common-draw-settings-form"
				form={functionDrawForm}
				submitter={false}
				onValuesChange={(_, values) => {
					updateAppSettings(
						AppSettingsGroup.FunctionDraw,
						values,
						true,
						true,
						true,
						true,
						false,
					);
				}}
				layout="horizontal"
			>
				<Spin spinning={appSettingsLoading}>
					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSwitch
								name="lockDrawTool"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.functionSettings.screenshotSettings.lockDrawTool" />
										}
									/>
								}
							/>
						</Col>

						<Col span={12}>
							<ProFormSwitch
								name="enableSliderChangeWidth"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.commonSettings.draw.enableSliderChangeWidth" />
										}
										tooltipTitle={
											<FormattedMessage id="settings.commonSettings.draw.enableSliderChangeWidth.tip" />
										}
									/>
								}
							/>
						</Col>

						<Col span={12}>
							<ProFormSwitch
								name="toolIndependentStyle"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.commonSettings.draw.toolIndependentStyle" />
										}
										tooltipTitle={
											<FormattedMessage id="settings.commonSettings.draw.toolIndependentStyle.tip" />
										}
									/>
								}
							/>
						</Col>
					</Row>

					<Row gutter={token.marginLG}>
						<Col span={24}>
							<ProFormSelect
								name="disableQuickSelectElementToolList"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.functionSettings.drawSettings.disableQuickSelectElementToolList" />
										}
										tooltipTitle={
											<FormattedMessage id="settings.functionSettings.drawSettings.disableQuickSelectElementToolList.tip" />
										}
									/>
								}
								mode="multiple"
								options={disableQuickSelectElementToolListOptions}
							/>
						</Col>
					</Row>
				</Spin>
			</ProForm>
			</div>

			<div style={{ display: activeTab === "fixedContentSettings" ? undefined : "none" }}>
			<Divider />

			<GroupTitle
				id="fixedContentSettings"
				extra={
					<ResetSettingsButton
						title={
							<FormattedMessage id="settings.functionSettings.fixedContentSettings" />
						}
						appSettingsGroup={AppSettingsGroup.FunctionFixedContent}
					/>
				}
			>
				<FormattedMessage id="settings.functionSettings.fixedContentSettings" />
			</GroupTitle>

			<Spin spinning={appSettingsLoading}>
				<ProForm
					form={fixedContentForm}
					onValuesChange={(_, values) => {
						updateAppSettings(
							AppSettingsGroup.FunctionFixedContent,
							values,
							true,
							true,
							true,
							true,
							false,
						);
					}}
					submitter={false}
					layout="horizontal"
				>
					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSwitch
								name="zoomWithMouse"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.fixedContentSettings.zoomWithMouse" />
								}
							/>
						</Col>

						<Col span={12}>
							<ProFormSelect
								name="initialPosition"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.fixedContentSettings.initialPosition" />
								}
								options={initialPositionOptions}
							/>
						</Col>

						{isReadyStatus?.(PLUGIN_ID_RAPID_OCR) && (
							<Col span={12}>
								<ProFormSwitch
									label={
										<FormattedMessage id="settings.functionSettings.fixedContentSettings.autoOcr" />
									}
									name="autoOcr"
									layout="horizontal"
								/>
							</Col>
						)}

						<Col span={12}>
							<ProFormSwitch
								name="autoResizeWindow"
								layout="horizontal"
								label={
									<IconLabel
										label={
											<FormattedMessage id="settings.functionSettings.fixedContentSettings.autoResizeWindow" />
										}
										tooltipTitle={
											<FormattedMessage id="settings.functionSettings.fixedContentSettings.autoResizeWindow.tip" />
										}
									/>
								}
							/>
						</Col>

						<Col span={12}>
							<ProFormSwitch
								label={
									<FormattedMessage id="settings.functionSettings.fixedContentSettings.autoCopyToClipboard" />
								}
								name="autoCopyToClipboard"
								layout="horizontal"
							/>
						</Col>
					</Row>
				</ProForm>
			</Spin>

			</div>

			<div style={{ display: activeTab === "ocrSettings" ? undefined : "none" }}>
			<GroupTitle
					id="ocrSettings"
						extra={
							<ResetSettingsButton
								title={
									<FormattedMessage id="settings.functionSettings.ocrSettings" />
								}
								appSettingsGroup={AppSettingsGroup.FunctionOcr}
							/>
						}
					>
						<FormattedMessage id="settings.functionSettings.ocrSettings" />
					</GroupTitle>

					<Spin spinning={appSettingsLoading}>
						<ProForm
							form={functionOcrForm}
							onValuesChange={(_, values) => {
								updateAppSettings(
									AppSettingsGroup.FunctionOcr,
									values,
									true,
									true,
									true,
									true,
									false,
								);
							}}
							submitter={false}
							layout="vertical"
						>
							<Row gutter={token.marginLG}>
								<Col span={12}>
									<ProFormSelect
										label={
											<IconLabel
												label={
													<FormattedMessage id="settings.systemSettings.screenshotSettings.ocrModel" />
												}
											/>
										}
										name="ocrModel"
										options={ocrModelOptions}
									/>
								</Col>

								{isReadyStatus?.(PLUGIN_ID_AI_CHAT) && (
									<>
										<Col span={12}>
											<ProFormSelect
												name="htmlVisionModel"
												label={
													<IconLabel
														label={
															<FormattedMessage id="settings.functionSettings.ocrSettings.htmlVisionModel" />
														}
														tooltipTitle={
															<FormattedMessage id="settings.functionSettings.ocrSettings.htmlVisionModel.tip" />
														}
													/>
												}
												layout="vertical"
												options={htmlVisionModelOptions}
												allowClear={false}
											/>
										</Col>
										<Col span={24}>
											<ProFormTextArea
												name="htmlVisionModelSystemPrompt"
												label={
													<IconLabel
														label={
															<FormattedMessage id="settings.functionSettings.ocrSettings.htmlVisionModelSystemPrompt" />
														}
													/>
												}
												fieldProps={{
													autoSize: {
														minRows: 1,
														maxRows: 1,
													},
												}}
											/>
										</Col>
										<Col span={24}>
											<ProFormTextArea
												name="markdownVisionModelSystemPrompt"
												label={
													<IconLabel
														label={
															<FormattedMessage id="settings.functionSettings.ocrSettings.markdownVisionModelSystemPrompt" />
														}
													/>
												}
												fieldProps={{
													autoSize: {
														minRows: 1,
														maxRows: 1,
													},
												}}
											/>
										</Col>
									</>
								)}
														</Row>

						<Divider style={{ margin: "12px 0" }} />
						<Typography.Text strong>
							PaddleOCR 云端识别（可选）
						</Typography.Text>
						<Typography.Paragraph type="secondary" style={{ fontSize: 12 }}>
							开启后可在「文本识别」时调用 PaddleOCR 云端 API，识别效果更佳。可设置引擎优先级。
						</Typography.Paragraph>
						<Flex align="center" gap={8} style={{ marginBottom: 12 }}>
							<Button
								onClick={() => {
									setPaddlePasteText("");
									setPaddleParseTip(null);
									setPaddlePasteOpen(true);
								}}
							>
								粘贴配置自动识别
							</Button>
							{paddleParseTip && (
								<Typography.Text type="success" style={{ fontSize: 12 }}>
									{paddleParseTip}
								</Typography.Text>
							)}
						</Flex>
						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSwitch
									name="enablePaddleOcr"
									label="启用 PaddleOCR（云端）"
								/>
							</Col>
							<Col span={12}>
								<ProFormSelect
									name="ocrPriority"
									label="OCR 引擎优先级"
									options={[
										{ label: "本地 RapidOCR 优先", value: "local" },
										{ label: "云端 PaddleOCR 优先", value: "paddle" },
									]}
								/>
							</Col>
							<Col span={12}>
								<ProFormText
									name="paddleOcrApiUrl"
									label="PaddleOCR API 地址"
									placeholder="https://paddleocr.aistudio-app.com/api/v2/ocr/jobs"
								/>
							</Col>
							<Col span={12}>
								<ProFormText.Password
									name="paddleOcrToken"
									label="PaddleOCR API Token"
									placeholder="请输入 Token"
								/>
							</Col>
							<Col span={12}>
								<ProFormText
									name="paddleOcrModel"
									label="PaddleOCR 模型"
									placeholder="PaddleOCR-VL-1.6"
								/>
							</Col>
						</Row>

						<Modal
							title="粘贴 PaddleOCR 配置自动识别"
							open={paddlePasteOpen}
							onOk={() => {
								const text = paddlePasteText;
								const url = text.match(
									/(?:JOB_URL|API_URL|URL)\s*[:=]\s*["']?([^"'\s]+)["']?/i,
								)?.[1];
								const token = text.match(
									/(?:TOKEN|API_KEY|KEY)\s*[:=]\s*["']?([^"'\s]+)["']?/i,
								)?.[1];
								const model = text.match(
									/MODEL\s*[:=]\s*["']?([^"'\s]+)["']?/i,
								)?.[1];
								const parsed: Record<string, string> = {};
								if (url) parsed.paddleOcrApiUrl = url;
								if (token) parsed.paddleOcrToken = token;
								if (model) parsed.paddleOcrModel = model;
								if (Object.keys(parsed).length === 0) {
									setPaddleParseTip(
										"未识别到有效配置，请粘贴 JOB_URL / TOKEN / MODEL 三行",
									);
									return;
								}
								functionOcrForm.setFieldsValue(parsed);
								updateAppSettings(
									AppSettingsGroup.FunctionOcr,
									parsed,
									false,
									true,
									true,
									true,
									false,
								);
								const tipParts: string[] = [];
								if (url) tipParts.push("API地址 ✓");
								if (token) tipParts.push("Token ✓");
								if (model) tipParts.push("模型 ✓");
								setPaddleParseTip("已自动识别并填入：" + tipParts.join(" "));
								setPaddlePasteOpen(false);
							}}
							onCancel={() => setPaddlePasteOpen(false)}
						>
							<Typography.Paragraph type="secondary" style={{ fontSize: 12 }}>
								粘贴以下格式的内容，将自动识别并填入上方配置：
							</Typography.Paragraph>
							<Input.TextArea
								value={paddlePasteText}
								onChange={(e) => setPaddlePasteText(e.target.value)}
								rows={6}
								placeholder={'JOB_URL = "https://paddleocr.aistudio-app.com/api/v2/ocr/jobs"\nTOKEN = "xxx"\nMODEL = "PaddleOCR-VL-1.6"'}
							/>
						</Modal>
					</ProForm>
				</Spin>
			</div>

			<div style={{ display: activeTab === "translationSettings" ? undefined : "none" }}>
				<GroupTitle
					id="translationSettings"
						extra={
							<ResetSettingsButton
								title={
									<FormattedMessage id="settings.functionSettings.translationSettings" />
								}
								appSettingsGroup={AppSettingsGroup.FunctionTranslation}
							/>
						}
					>
						<FormattedMessage id="settings.functionSettings.translationSettings" />
					</GroupTitle>

						<Spin spinning={appSettingsLoading}>
												<TranslationConfig />

												<ProForm
													form={translationForm}
													onValuesChange={(_, values) => {
														updateAppSettings(
															AppSettingsGroup.FunctionTranslation,
															values,
															true,
															true,
															true,
															true,
															false,
														);
													}}
													submitter={false}
												>
												<div
								style={{
									border: "1px solid #d3adf7",
									background: "#fdfaff",
									borderRadius: 8,
									padding: "10px 14px",
									marginBottom: 16,
									color: "#722ed1",
									fontSize: 13,
								}}
							>
								原生支持，默认开启。内置 5 个云翻译引擎，无需安装插件。未配置百度/智谱时默认调用
								<b>腾讯通天塔</b>。拖动下方列表可调整引擎优先级，失败自动切换下一个。
							</div>

							<Row gutter={token.marginLG}>
								<Col span={12}>
									<ProFormSwitch
										name="enableCloudTranslation"
										label="启用云端翻译（原生云引擎）"
									/>
								</Col>
								<Col span={12}>
									<ProFormSwitch
										name="optimizeAiTranslationLayout"
										label={
											<IconLabel
												label={
													<FormattedMessage id="settings.functionSettings.translationSettings.optimizeAiTranslationLayout" />
												}
												tooltipTitle={
													<FormattedMessage id="settings.functionSettings.translationSettings.optimizeAiTranslationLayout.tip" />
												}
											/>
										}
										layout="vertical"
									/>
								</Col>
							</Row>

							<Row gutter={token.marginLG} style={{ marginTop: 8 }}>
								<Col span={12}>
									<ProForm.Item
										label="默认引擎（拖拽排序后自动跟随第一位）"
										layout="horizontal"
									>
										<Select
											id="cloudEngineDefaultSelect"
											value={cloudEngineOrder[0]}
											onChange={(value) => {
												const next = [value, ...cloudEngineOrder.filter((v) => v !== value)];
												updateAppSettings(
													AppSettingsGroup.FunctionTranslation,
													{ cloudTranslationConfig: { ...translationCloudConfig, engineOrder: next } },
													true,
													true,
													true,
													true,
													false,
												);
											}}
											options={cloudEngineOrder.map((v) => ({
												value: v,
												label: CLOUD_ENGINE_META[v]?.label ?? v,
											}))}
										/>
									</ProForm.Item>
								</Col>
							</Row>

							<Divider style={{ margin: "16px 0" }} />

							<Flex justify="space-between" align="center" style={{ marginBottom: 12 }}>
								<Typography.Text strong>翻译引擎优先级</Typography.Text>
								<Typography.Text type="secondary" style={{ fontSize: 12 }}>
									上下拖动调整调用顺序，前面的引擎失败时自动尝试下一个
								</Typography.Text>
							</Flex>

							<List
								className="cloud-engine-list"
								dataSource={cloudEngineOrder}
								renderItem={(engine, index) => {
									const meta = CLOUD_ENGINE_META[engine] ?? {
										label: engine,
										needKey: false,
										tag: "default",
									};
									const isFirst = index === 0;
									const isConfigOpen = cloudEngineConfigOpen === engine;
									return (
										<Flex
											key={engine}
											align="center"
											gap={12}
											style={{
												padding: "10px 14px",
												border: isFirst
													? "1px solid #d3adf7"
													: "1px solid #f0f0f0",
												borderRadius: 8,
												marginBottom: 8,
												background: isFirst ? "#fdfaff" : "#fff",
												cursor: "grab",
											}}
											onDragStart={(e) => {
												e.dataTransfer.setData("text/plain", engine);
											}}
											onDrop={(e) => {
												e.preventDefault();
												const dragged = e.dataTransfer.getData("text/plain") as TranslationApiType;
												if (!dragged || dragged === engine) return;
												const next = [...cloudEngineOrder];
												const from = next.indexOf(dragged);
												next.splice(from, 1);
												const to = next.indexOf(engine);
												next.splice(to, 0, dragged);
												updateAppSettings(
													AppSettingsGroup.FunctionTranslation,
													{ cloudTranslationConfig: { ...translationCloudConfig, engineOrder: next } },
													true,
													true,
													true,
													true,
													false,
												);
											}}
											onDragOver={(e) => e.preventDefault()}
										>
											<span style={{ color: "#bbb", fontSize: 16 }}>☰</span>
											<div style={{ flex: 1 }}>
												<div style={{ fontSize: 14, fontWeight: 500 }}>
													{meta.label}
													{meta.needKey && (
														<Tag color="orange" style={{ marginLeft: 8 }}>
															需配置
														</Tag>
													)}
													{!meta.needKey && (
														<Tag color="green" style={{ marginLeft: 8 }}>
															免 Key
														</Tag>
													)}
													{isFirst && (
														<Tag color="purple" style={{ marginLeft: 4 }}>
															★ 默认
														</Tag>
													)}
												</div>
												<div style={{ fontSize: 12, color: "#8c8c8c", marginTop: 2 }}>
													{meta.desc}
												</div>
											</div>
											{meta.needKey && (
												<Tooltip title="配置 API Key">
													<Button
														size="small"
														icon={<span>✎</span>}
														onClick={() =>
															setCloudEngineConfigOpen(
																isConfigOpen ? undefined : engine,
															)
														}
													/>
												</Tooltip>
											)}
										</Flex>
									);
								}}
							/>

							{cloudEngineConfigOpen === TranslationApiType.Baidu && (
								<div
									style={{
										border: "1px dashed #d3adf7",
										background: "#faf8ff",
										borderRadius: 10,
										padding: "20px 24px",
										marginTop: 4,
									}}
								>
									<div style={{ fontWeight: 600, color: "#722ed1", marginBottom: 14 }}>
										🔑 百度翻译 API 配置
									</div>
									<Row gutter={16} style={{ marginBottom: 12 }}>
										<Col span={10}>
											<Input
												placeholder="请输入百度翻译 AppID"
												value={translationCloudConfig.baiduAppId}
												onChange={(e) =>
													updateAppSettings(
														AppSettingsGroup.FunctionTranslation,
														{
															cloudTranslationConfig: {
																...translationCloudConfig,
																baiduAppId: e.target.value,
															},
														},
														true,
														true,
														true,
														true,
														false,
													)
												}
											/>
										</Col>
										<Col span={10}>
											<Input.Password
												placeholder="请输入百度翻译密钥"
												value={translationCloudConfig.baiduAppKey}
												onChange={(e) =>
													updateAppSettings(
														AppSettingsGroup.FunctionTranslation,
														{
															cloudTranslationConfig: {
																...translationCloudConfig,
																baiduAppKey: e.target.value,
															},
														},
														true,
														true,
														true,
														true,
														false,
													)
												}
											/>
										</Col>
									</Row>
									<Flex justify="space-between" align="center">
										<Typography.Text type="secondary" style={{ fontSize: 12 }}>
											接口地址已内置 https://api.fanyi.baidu.com/api/trans/vip/translate
										</Typography.Text>
										<Button
											size="small"
											onClick={() =>
												cloudEngineTest(TranslationApiType.Baidu)
											}
										>
											测试连通性
										</Button>
									</Flex>
								</div>
							)}

							{cloudEngineConfigOpen === TranslationApiType.BigModel && (
								<div
									style={{
										border: "1px dashed #d3adf7",
										background: "#faf8ff",
										borderRadius: 10,
										padding: "20px 24px",
										marginTop: 4,
									}}
								>
									<div style={{ fontWeight: 600, color: "#722ed1", marginBottom: 14 }}>
										🔑 智谱 GLM API 配置
									</div>
									<Row gutter={16} style={{ marginBottom: 12 }}>
										<Col span={14}>
											<Input.Password
												placeholder="请输入智谱 API Key（注意本地明文保存）"
												value={translationCloudConfig.bigmodelKey}
												onChange={(e) =>
													updateAppSettings(
														AppSettingsGroup.FunctionTranslation,
														{
															cloudTranslationConfig: {
																...translationCloudConfig,
																bigmodelKey: e.target.value,
															},
														},
														true,
														true,
														true,
														true,
														false,
													)
												}
											/>
										</Col>
									</Row>
									<Flex justify="space-between" align="center">
										<Typography.Text type="secondary" style={{ fontSize: 12 }}>
											接口地址已内置 https://open.bigmodel.cn/api/paas/v4/chat/completions
										</Typography.Text>
										<Button
											size="small"
											onClick={() =>
												cloudEngineTest(TranslationApiType.BigModel)
											}
										>
											测试连通性
										</Button>
									</Flex>
								</div>
																)}

																<Divider style={{ margin: "16px 0" }} />

																<Row gutter={token.marginLG}>
									<Col span={24}>
										<ProFormTextArea
											label={
												<IconLabel
													label={
														<FormattedMessage id="settings.functionSettings.translationSettings.chatPrompt" />
													}
													tooltipTitle={
														<FormattedMessage id="settings.functionSettings.translationSettings.chatPrompt.tip" />
													}
												/>
											}
											layout="horizontal"
											name="translationSystemPrompt"
											rules={[
												{
													required: true,
													message: intl.formatMessage({
														id: "settings.functionSettings.translationSettings.chatPrompt.required",
													}),
												},
											]}
											fieldProps={{
												autoSize: {
													minRows: 1,
													maxRows: 1,
												},
											}}
										/>
									</Col>
								</Row>
							</ProForm>
						</Spin>
				</div>

			<div style={{ display: activeTab === "chatSettings" ? undefined : "none" }}>
			{(isReadyStatus?.(PLUGIN_ID_TRANSLATE) ||
				isReadyStatus?.(PLUGIN_ID_AI_CHAT)) && (
				<>
					<Divider />

					<GroupTitle
						id="chatSettings"
						extra={
							<ResetSettingsButton
								title={
									<FormattedMessage id="settings.functionSettings.chatSettings" />
								}
								appSettingsGroup={AppSettingsGroup.FunctionChat}
							/>
						}
					>
						<FormattedMessage id="settings.functionSettings.chatSettings" />
					</GroupTitle>

					<Spin spinning={appSettingsLoading}>
						<ProForm
							form={functionForm}
							onValuesChange={(_, values) => {
								updateAppSettings(
									AppSettingsGroup.FunctionChat,
									values,
									true,
									true,
									true,
									true,
									false,
								);
							}}
							submitter={false}
						>
							{isReadyStatus?.(PLUGIN_ID_AI_CHAT) && (
								<Row gutter={token.marginLG}>
									<Col span={12}>
										<ProForm.Item
											label={
												<IconLabel
													label={
														<FormattedMessage id="settings.functionSettings.chatSettings.autoCreateNewSession" />
													}
												/>
											}
											layout="horizontal"
											name="autoCreateNewSession"
											valuePropName="checked"
										>
											<Switch />
										</ProForm.Item>
									</Col>

									<Col span={12}>
										<ProForm.Item
											label={
												<IconLabel
													label={
														<FormattedMessage id="settings.functionSettings.chatSettings.autoCreateNewSessionOnCloseWindow" />
													}
												/>
											}
											layout="horizontal"
											name="autoCreateNewSessionOnCloseWindow"
											valuePropName="checked"
										>
											<Switch />
										</ProForm.Item>
									</Col>
								</Row>
							)}

							<Row gutter={token.marginLG}>
								<Col span={24}>
									<ProFormList
										name="chatApiConfigList"
										label={
											<IconLabel
												label={
													<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig" />
												}
												tooltipTitle={
													<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.tip" />
												}
											/>
										}
										creatorButtonProps={{
											creatorButtonText: intl.formatMessage({
												id: "settings.functionSettings.chatSettings.apiConfig.add",
											}),
										}}
										actionRender={(...params) => {
											const [field, , defaultActionDom] = params;
											return [
												defaultActionDom,
												<TestChat
													key="test-chat"
													config={
														functionForm.getFieldValue("chatApiConfigList")[
															field.name
														]
													}
												/>,
											];
										}}
										className="api-config-list"
										min={0}
										itemRender={({ listDom, action }) => (
											<Flex align="end" justify="space-between">
												{listDom}
												<div>{action}</div>
											</Flex>
										)}
										creatorRecord={() => ({
											api_uri: "",
											api_key: "",
											api_model: "",
											model_name: "",
										})}
									>
										<Row gutter={token.marginLG} style={{ width: "100%" }}>
											<Col span={12}>
												<ProFormText
													name="model_name"
													label={
														<IconLabel
															label={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.modelName" />
															}
															tooltipTitle={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.modelName.tip" />
															}
														/>
													}
													rules={[
														{
															required: true,
															message: intl.formatMessage({
																id: "settings.functionSettings.chatSettings.apiConfig.modelName.required",
															}),
														},
													]}
												/>
											</Col>
										</Row>
										<Row gutter={token.marginLG}>
											<Col span={12}>
												<ProFormText
													name="api_uri"
													label={
														<IconLabel
															label={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.apiUri" />
															}
															tooltipTitle={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.apiUri.tip" />
															}
														/>
													}
													rules={[
														{
															required: true,
															message: intl.formatMessage({
																id: "settings.functionSettings.chatSettings.apiConfig.apiUri.required",
															}),
														},
													]}
												/>
											</Col>
											<Col span={12}>
												<ProFormText.Password
													name="api_key"
													label={
														<IconLabel
															label={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.apiKey" />
															}
															tooltipTitle={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.apiKey.tip" />
															}
														/>
													}
													rules={[
														{
															required: true,
															message: intl.formatMessage({
																id: "settings.functionSettings.chatSettings.apiConfig.apiKey.required",
															}),
														},
													]}
												/>
											</Col>
											<Col span={12}>
												<ProFormText
													name="api_model"
													label={
														<IconLabel
															label={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.apiModel" />
															}
															tooltipTitle={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.apiModel.tip" />
															}
														/>
													}
													rules={[
														{
															required: true,
															message: intl.formatMessage({
																id: "settings.functionSettings.chatSettings.apiConfig.apiModel.required",
															}),
														},
													]}
												/>
											</Col>
										</Row>
										<Row gutter={token.marginLG}>
											<Col span={12}>
												<ProFormSwitch
													name="support_thinking"
													label={
														<IconLabel
															label={
																<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.supportThinking" />
															}
														/>
													}
												/>
											</Col>
											{isReadyStatus?.(PLUGIN_ID_AI_CHAT) && (
												<Col span={12}>
													<ProFormSwitch
														name="support_vision"
														label={
															<IconLabel
																label={
																	<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.supportVision" />
																}
																tooltipTitle={
																	<FormattedMessage id="settings.functionSettings.chatSettings.apiConfig.supportVision.tip" />
																}
															/>
														}
													/>
												</Col>
											)}
										</Row>
									</ProFormList>
								</Col>
							</Row>
						</ProForm>
					</Spin>
				</>
			)}
			</div>

			<Divider />

			<div style={{ display: activeTab === "fullScreenDrawSettings" ? undefined : "none" }}>
			<GroupTitle
				id="fullScreenDrawSettings"
				extra={
					<ResetSettingsButton
						title={
							<FormattedMessage id="settings.functionSettings.fullScreenDrawSettings" />
						}
						appSettingsGroup={AppSettingsGroup.FunctionFullScreenDraw}
					/>
				}
			>
				<FormattedMessage id="settings.functionSettings.fullScreenDrawSettings" />
			</GroupTitle>

			<Spin spinning={appSettingsLoading}>
				<ProForm
					form={fullScreenDrawForm}
					onValuesChange={(_, values) => {
						updateAppSettings(
							AppSettingsGroup.FunctionFullScreenDraw,
							values,
							true,
							true,
							true,
							true,
							false,
						);
					}}
					submitter={false}
					layout="horizontal"
				>
					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSelect
								name="defaultTool"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.fullScreenDrawSettings.defaultTool" />
								}
								options={fullScreenDrawDefaultToolOptions}
							/>
						</Col>
					</Row>
				</ProForm>
			</Spin>

			</div>

			<div style={{ display: activeTab === "videoRecordSettings" ? undefined : "none" }}>
			<Divider />

			<div hidden={!isReadyStatus?.(PLUGIN_ID_FFMPEG)}>
				<GroupTitle
					id="videoRecordSettings"
					extra={
						<ResetSettingsButton
							title={
								<FormattedMessage id="settings.functionSettings.videoRecordSettings" />
							}
							appSettingsGroup={AppSettingsGroup.FunctionVideoRecord}
						/>
					}
				>
					<FormattedMessage id="settings.functionSettings.videoRecordSettings" />
				</GroupTitle>

				<Spin spinning={appSettingsLoading}>
					<ProForm
						form={videoRecordForm}
						onValuesChange={(_, values) => {
							// 处理颜色值转换
							if (typeof values.keyDisplayBackgroundColor === "object") {
								values.keyDisplayBackgroundColor = (
									values.keyDisplayBackgroundColor as AggregationColor
								).toRgbString();
							}
							if (typeof values.keyDisplayTextColor === "object") {
								values.keyDisplayTextColor = (
									values.keyDisplayTextColor as AggregationColor
								).toHexString();
							}

							updateAppSettings(
								AppSettingsGroup.FunctionVideoRecord,
								values,
								true,
								true,
								true,
								true,
								false,
							);
						}}
						submitter={false}
						layout="horizontal"
					>
						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSelect
									name="videoMaxSize"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.videoMaxSize" />
									}
									options={videoMaxSizeOptions}
								/>
							</Col>

							<Col span={12}>
								<ProFormSelect
									name="frameRate"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.frameRate" />
									}
									options={[
										{
											label: "10",
											value: 10,
										},
										{
											label: "15",
											value: 15,
										},
										{
											label: "24",
											value: 24,
										},
										{
											label: "30",
											value: 30,
										},
										{
											label: "60",
											value: 60,
										},
										{
											label: "120",
											value: 120,
										},
										{
											label: "83",
											value: 83,
										},
										{
											label: "42",
											value: 42,
										},
									]}
								/>
							</Col>
						</Row>

						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSelect
									name="gifMaxSize"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.gifMaxSize" />
									}
									options={gifMaxSizeOptions}
								/>
							</Col>

							<Col span={12}>
								<ProFormSelect
									name="gifFrameRate"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.gifFrameRate" />
									}
									options={[
										{
											label: "10",
											value: 10,
										},
										{
											label: "15",
											value: 15,
										},
										{
											label: "24",
											value: 24,
										},
									]}
								/>
							</Col>

							<Col span={12}>
								<ProFormSelect
									name="gifFormat"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.gifFormat" />
									}
									options={gifFormatOptions}
								/>
							</Col>
						</Row>
						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSelect
									name="microphoneDeviceName"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.microphoneDeviceName" />
									}
									options={microphoneDeviceNameOptions}
								/>
							</Col>
						</Row>
						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSelect
									name="encoder"
									layout="horizontal"
									label={
										<IconLabel
											label={
												<FormattedMessage id="settings.functionSettings.videoRecordSettings.encoder" />
											}
											tooltipTitle={
												<FormattedMessage id="settings.functionSettings.videoRecordSettings.encoder.tip" />
											}
										/>
									}
									options={[
										{
											label: "Libx264 (CPU)",
											value: "libx264",
										},
										{
											label: "Libx265 (CPU)",
											value: "libx265",
										},
										...(currentPlatform === "windows"
											? [
													{
														label: "H264_AMF (AMD)",
														value: "h264_amf",
													},
													{
														label: "H264_NVENC (NVIDIA)",
														value: "h264_nvenc",
													},
												]
											: []),
									]}
								/>
							</Col>

							<Col span={12}>
								<ProFormSelect
									name="encoderPreset"
									layout="horizontal"
									label={
										<IconLabel
											label={
												<FormattedMessage id="settings.functionSettings.videoRecordSettings.encoderPreset" />
											}
											tooltipTitle={
												<FormattedMessage id="settings.functionSettings.videoRecordSettings.encoderPreset.tip" />
											}
										/>
									}
									options={encoderPresetOptions}
								/>
							</Col>

							<Col span={12}>
								<ProFormSwitch
									name="hwaccel"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.hwaccel" />
									}
								/>
							</Col>
						</Row>
						<Row gutter={token.marginLG}>
							<Col span={24}>
								<ProForm.Item
									name="saveDirectory"
									label={
										<IconLabel
											label={
												<FormattedMessage id="settings.functionSettings.videoRecordSettings.saveDirectory" />
											}
										/>
									}
									required={false}
								>
									<DirectoryInput />
								</ProForm.Item>
							</Col>
						</Row>

						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormSwitch
									name="enableExcludeFromCapture"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.enableExcludeFromCapture" />
									}
								/>
							</Col>
						</Row>

						<SubGroupTitle>
							<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplaySettings" />
						</SubGroupTitle>

						<Row gutter={token.marginLG} style={{ width: "100%" }}>
							<Col span={12} style={{ width: "100%" }}>
								<ProFormDigit
									name="keyDisplayFontSize"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayFontSize" />
									}
									style={{ width: "100%" }}
									min={8}
									max={64}
									fieldProps={{
										precision: 0,
									}}
								/>
							</Col>

							<Col span={12} style={{ width: "100%" }}>
								<ProFormDigit
									name="keyDisplayDuration"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayDuration" />
									}
									min={100}
									max={10000}
									style={{ width: "100%" }}
									fieldProps={{
										precision: 0,
										addonAfter: "ms",
									}}
								/>
							</Col>
						</Row>

						<Row gutter={token.marginLG}>
							<Col span={12}>
								<ProFormDigit
									name="keyDisplayMergeDuration"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayMergeDuration" />
									}
									min={0}
									max={2000}
									fieldProps={{
										precision: 0,
										addonAfter: "ms",
									}}
								/>
							</Col>
							<Col span={12}>
								<ProFormSelect
									name="keyDisplayDirection"
									layout="horizontal"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayDirection" />
									}
									options={[
										{
											label: (
												<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayDirection.horizontal" />
											),
											value: KeyDisplayDirection.Horizontal,
										},
										{
											label: (
												<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayDirection.vertical" />
											),
											value: KeyDisplayDirection.Vertical,
										},
									]}
								/>
							</Col>
							<Col span={12}>
								<ProForm.Item
									name="keyDisplayBackgroundColor"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayBackgroundColor" />
									}
									required={false}
								>
									<ColorPicker showText placement="bottom" />
								</ProForm.Item>
							</Col>

							<Col span={12}>
								<ProForm.Item
									name="keyDisplayTextColor"
									label={
										<FormattedMessage id="settings.functionSettings.videoRecordSettings.keyDisplayTextColor" />
									}
									required={false}
								>
									<ColorPicker showText placement="bottom" />
								</ProForm.Item>
							</Col>
						</Row>
					</ProForm>
				</Spin>

				<Divider />
				</div>
				</div>

				<div style={{ display: activeTab === "trayIconSettings" ? undefined : "none" }}>
			<GroupTitle
				id="trayIconSettings"
				extra={
					<ResetSettingsButton
						title={
							<FormattedMessage id="settings.functionSettings.trayIconSettings" />
						}
						appSettingsGroup={AppSettingsGroup.FunctionTrayIcon}
					/>
				}
			>
				<FormattedMessage id="settings.functionSettings.trayIconSettings" />
			</GroupTitle>

			<Spin spinning={appSettingsLoading}>
				<ProForm
					form={trayIconForm}
					onValuesChange={(_, values) => {
						updateAppSettings(
							AppSettingsGroup.FunctionTrayIcon,
							values,
							true,
							true,
							false,
							true,
							false,
						);
					}}
					submitter={false}
					layout="horizontal"
				>
					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSelect
								name="iconClickAction"
								label={
									<FormattedMessage id="settings.functionSettings.trayIconSettings.iconClickAction" />
								}
								options={trayIconClickActionOptions}
							/>
						</Col>
					</Row>
				</ProForm>
			</Spin>

			<Divider />
			</div>

			<div style={{ display: activeTab === "globalShortcutSettings" ? undefined : "none" }}>
			<GroupTitle
				id="globalShortcutSettings"
				extra={
					<ResetSettingsButton
						title={
							<FormattedMessage id="settings.functionSettings.globalShortcutSettings" />
						}
						appSettingsGroup={AppSettingsGroup.FunctionGlobalShortcut}
					/>
				}
			>
				<FormattedMessage id="settings.functionSettings.globalShortcutSettings" />
			</GroupTitle>

			<Spin spinning={appSettingsLoading}>
				<ProForm
					form={functionGlobalShortcutForm}
					onValuesChange={(_, values) => {
						updateAppSettings(
							AppSettingsGroup.FunctionGlobalShortcut,
							values,
							true,
							true,
							false,
							true,
							false,
						);
					}}
					submitter={false}
					layout="horizontal"
				>
					<Row gutter={token.marginLG}>
						<Col span={12}>
							<ProFormSwitch
								name="disableOnFocusedFullScreenWindow"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.globalShortcutSettings.disableOnFocusedFullScreenWindow" />
								}
							/>
						</Col>
					</Row>
				</ProForm>
			</Spin>

			<Divider />
			</div>

			<div style={{ display: activeTab === "outputSettings" ? undefined : "none" }}>
			<GroupTitle
				id="outputSettings"
				extra={
					<ResetSettingsButton
						title={
							<FormattedMessage id="settings.functionSettings.outputSettings" />
						}
						appSettingsGroup={AppSettingsGroup.FunctionOutput}
					/>
				}
			>
				<FormattedMessage id="settings.functionSettings.outputSettings" />
			</GroupTitle>

			<Alert
				message={
					<Typography>
						<Row>
							<Col span={24}>
								<FormattedMessage id="settings.functionSettings.outputSettings.variables" />
							</Col>
							<Col span={12}>
								<FormattedMessage id="settings.functionSettings.outputSettings.variables.date" />
								<code>{"{{YYYY-MM-DD_HH-mm-ss}}"}</code>
							</Col>
							<Col span={12}>
								<FormattedMessage id="settings.functionSettings.outputSettings.variables.focusedWindowAppName" />
								<code>{FOCUS_WINDOW_APP_NAME_ENV_VARIABLE}</code>
							</Col>
						</Row>
					</Typography>
				}
				type="info"
				style={{ marginBottom: token.margin }}
			/>

			<Spin spinning={appSettingsLoading}>
				<ProForm
					form={outputForm}
					onValuesChange={(_, values) => {
						updateAppSettings(
							AppSettingsGroup.FunctionOutput,
							values,
							true,
							true,
							true,
							true,
							false,
						);
					}}
					submitter={false}
					layout="horizontal"
				>
					<Row gutter={token.marginLG}>
						<Col span={24}>
							<ProFormText
								name="manualSaveFileNameFormat"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.outputSettings.manualSaveFileNameFormat" />
								}
							/>
						</Col>

						<ProFormDependency<{ manualSaveFileNameFormat: string }>
							name={["manualSaveFileNameFormat"]}
						>
							{({ manualSaveFileNameFormat }) => {
								const text = generateImageFileName(manualSaveFileNameFormat);
								return (
									<Col span={24}>
										<ProFormText
											layout="horizontal"
											readonly
											label={
												<FormattedMessage id="settings.functionSettings.outputSettings.manualSaveFileNameFormatPreview" />
											}
											fieldProps={{
												value: text,
											}}
										/>
									</Col>
								);
							}}
						</ProFormDependency>

						<Col span={24}>
							<ProFormText
								name="autoSaveFileNameFormat"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.outputSettings.autoSaveFileNameFormat" />
								}
							/>
						</Col>

						<ProFormDependency<{ autoSaveFileNameFormat: string }>
							name={["autoSaveFileNameFormat"]}
						>
							{({ autoSaveFileNameFormat }) => {
								const text = generateImageFileName(autoSaveFileNameFormat);
								return (
									<Col span={24}>
										<ProFormText
											layout="horizontal"
											readonly
											label={
												<FormattedMessage id="settings.functionSettings.outputSettings.autoSaveFileNameFormatPreview" />
											}
											fieldProps={{
												value: text,
											}}
										/>
									</Col>
								);
							}}
						</ProFormDependency>

						<Col span={24}>
							<ProFormText
								name="fastSaveFileNameFormat"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.outputSettings.fastSaveFileNameFormat" />
								}
							/>
						</Col>

						<ProFormDependency<{ fastSaveFileNameFormat: string }>
							name={["fastSaveFileNameFormat"]}
						>
							{({ fastSaveFileNameFormat }) => {
								const text = generateImageFileName(fastSaveFileNameFormat);
								return (
									<Col span={24}>
										<ProFormText
											layout="horizontal"
											readonly
											label={
												<FormattedMessage id="settings.functionSettings.outputSettings.fastSaveFileNameFormatPreview" />
											}
											fieldProps={{
												value: text,
											}}
										/>
									</Col>
								);
							}}
						</ProFormDependency>

						<Col span={24}>
							<ProFormText
								name="focusedWindowFileNameFormat"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.outputSettings.focusedWindowFileNameFormat" />
								}
							/>
						</Col>

						<ProFormDependency<{ focusedWindowFileNameFormat: string }>
							name={["focusedWindowFileNameFormat"]}
						>
							{({ focusedWindowFileNameFormat }) => {
								const text = generateImageFileName(focusedWindowFileNameFormat);
								return (
									<Col span={24}>
										<ProFormText
											layout="horizontal"
											readonly
											label={
												<FormattedMessage id="settings.functionSettings.outputSettings.focusedWindowFileNameFormatPreview" />
											}
											fieldProps={{
												value: text,
											}}
										/>
									</Col>
								);
							}}
						</ProFormDependency>

						<Col span={24}>
							<ProFormText
								name="fullScreenFileNameFormat"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.outputSettings.fullScreenFileNameFormat" />
								}
							/>
						</Col>

						<ProFormDependency<{ fullScreenFileNameFormat: string }>
							name={["fullScreenFileNameFormat"]}
						>
							{({ fullScreenFileNameFormat }) => {
								const text = generateImageFileName(fullScreenFileNameFormat);
								return (
									<Col span={24}>
										<ProFormText
											layout="horizontal"
											readonly
											label={
												<FormattedMessage id="settings.functionSettings.outputSettings.fullScreenFileNameFormatPreview" />
											}
											fieldProps={{
												value: text,
											}}
										/>
									</Col>
								);
							}}
						</ProFormDependency>

						<Col span={24}>
							<ProFormText
								name="uploadToCloudSaveUrlFormat"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.outputSettings.uploadToCloudSaveUrlFormat" />
								}
							/>
						</Col>

						<ProFormDependency<{ uploadToCloudSaveUrlFormat: string }>
							name={["uploadToCloudSaveUrlFormat"]}
						>
							{({ uploadToCloudSaveUrlFormat }) => {
								const text = generateImageFileName(uploadToCloudSaveUrlFormat);
								return (
									<Col span={24}>
										<ProFormText
											layout="horizontal"
											readonly
											label={
												<FormattedMessage id="settings.functionSettings.outputSettings.uploadToCloudSaveUrlFormatPreview" />
											}
											fieldProps={{
												value: text,
											}}
										/>
									</Col>
								);
							}}
						</ProFormDependency>

						<Col span={24}>
							<ProFormText
								name="videoRecordFileNameFormat"
								layout="horizontal"
								label={
									<FormattedMessage id="settings.functionSettings.outputSettings.videoRecordFileNameFormat" />
								}
							/>
						</Col>

						<ProFormDependency<{ videoRecordFileNameFormat: string }>
							name={["videoRecordFileNameFormat"]}
						>
							{({ videoRecordFileNameFormat }) => {
								const text = generateImageFileName(videoRecordFileNameFormat);
								return (
									<Col span={24}>
										<ProFormText
											layout="horizontal"
											readonly
											label={
												<FormattedMessage id="settings.functionSettings.outputSettings.videoRecordFileNameFormatPreview" />
											}
											fieldProps={{
												value: text,
											}}
										/>
									</Col>
								);
							}}
						</ProFormDependency>
					</Row>
				</ProForm>
			</Spin>

			<style jsx>{`
                :global(.api-config-list .ant-pro-form-list-container) {
                    width: 100%;
                }
            `}</style>
			</div>
		</ContentWrap>
	);
};
