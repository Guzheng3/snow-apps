import type { ElementRect } from "./commands/screenshot";
import type { AppFunction, AppFunctionConfig } from "./components/appFunction";
import type {
	DrawToolbarKeyEventKey,
	DrawToolbarKeyEventValue,
} from "./components/drawToolbar";
import type {
	CommonKeyEventKey,
	CommonKeyEventValue,
} from "./core/commonKeyEvent";
import { DrawState } from "./draw";
import type { TranslationDomain, TranslationType } from "./servies/translation";
import type { ImageFormat } from "./utils/file";

export enum HistoryValidDuration {
	/** 用于测试，不对外暴露 */
	Test = 1,
	Day = 24 * 60 * 60 * 1000 * 1,
	Three = 24 * 60 * 60 * 1000 * 3,
	Week = 24 * 60 * 60 * 1000 * 7,
	Month = 24 * 60 * 60 * 1000 * 30,
	Forever = 0,
}

export enum VideoFormat {
	Mp4 = "Mp4",
	Gif = "Gif",
}

export enum VideoMaxSize {
	P2160 = "2160p",
	P1440 = "1440p",
	P1080 = "1080p",
	P720 = "720p",
	P480 = "480p",
}

export enum GifFormat {
	Gif = "gif",
	Apng = "apng",
	Webp = "webp",
}

export enum OcrDetectAfterAction {
	/** 不执行任何操�?*/
	None = "none",
	/** 复制文本 */
	CopyText = "copyText",
	/** 复制文本并关闭窗�?*/
	CopyTextAndCloseWindow = "copyTextAndCloseWindow",
	/** 文本识别-复制文本 */
	OcrDetectCopyText = "ocrDetectCopyText",
	/** 文本识别-复制文本并关闭窗�?*/
	OcrDetectCopyTextAndCloseWindow = "ocrDetectCopyTextAndCloseWindow",
}

export enum HdrColorAlgorithm {
	Linear = "Linear",
	None = "None",
}

export type ChatApiConfig = {
	api_uri: string;
	api_key: string;
	api_model: string;
	model_name: string;
	support_thinking: boolean;
	support_vision: boolean | undefined;
};

export enum TranslationApiType {
	DeepL = "translation_api_deepl",
	Google = "translation_api_google",
	Youdao = "translation_api_youdao",
	/** 金山词霸 (免Key) */
	ICiba = "translation_api_iciba",
	/** 腾讯通天�?(免Key) */
	Transmart = "translation_api_transmart",
	/** Yandex (免Key) */
	Yandex = "translation_api_yandex",
	/** 百度翻译 (需AppID+AppKey) */
	Baidu = "translation_api_baidu",
	/** 智谱GLM (需API Key) */
	BigModel = "translation_api_bigmodel",
}

export type TranslationApiConfig = {
	api_type: TranslationApiType;
	api_uri: string;
	api_key: string;
	/** 是否启用该翻�?API */
	enable: boolean;
	/** 优先级，数字越小越优�?*/
	priority: number;
	deepl_prefer_quality_optimized?: boolean;
};

/** 云翻译引擎优先级排序 */
export type CloudEngineOrder = TranslationApiType[];

/** 云翻译引擎配�?*/
export type CloudTranslationConfig = {
	baiduAppId: string;
	baiduAppKey: string;
	bigmodelKey: string;
	engineOrder: CloudEngineOrder;
};

export enum AppSettingsGroup {
	Common = "common",
	ThemeSkin = "themeSkin",
	CommonTrayIcon = "commonTrayIcon",
	FunctionDraw = "functionDraw",
	Cache = "cache_20250731",
	Screenshot = "screenshot",
	FixedContent = "fixedContent",
	DrawToolbarKeyEvent = "drawToolbarKeyEvent_20250526",
	CommonKeyEvent = "commonKeyEvent",
	AppFunction = "appFunction",
	Render = "render",
	SystemCommon = "systemCommon",
	SystemChat = "systemChat",
	SystemNetwork = "systemNetwork",
	SystemScreenshot = "systemScreenshot_20250627",
	SystemCore = "systemCore",
	SystemScrollScreenshot = "systemScrollScreenshot_20250628",
	FunctionChat = "functionChat",
	FunctionOcr = "functionOcr",
	FunctionTranslation = "functionTranslation",
	FunctionTranslationCache = "functionTranslationCache",
	FunctionScreenshot = "functionScreenshot",
	FunctionFullScreenDraw = "functionFullScreenDraw",
	FunctionOutput = "functionOutput_20250908",
	FunctionFixedContent = "functionFixedContent",
	FunctionVideoRecord = "functionVideoRecord",
	FunctionTrayIcon = "functionTrayIcon",
	FunctionGlobalShortcut = "functionGlobalShortcut",
}

export enum ShortcutKeyStatus {
	Registered = "registered",
	Unregistered = "unregistered",
	Error = "error",
	None = "none",
	PrintScreen = "printScreen",
}

export enum AppSettingsLanguage {
	ZHHans = "zh-Hans",
	ZHHant = "zh-Hant",
	EN = "en",
}

export enum AppSettingsControlNode {
	Circle = "circle",
	Polyline = "polyline",
}

export enum AppSettingsFixedContentInitialPosition {
	MonitorCenter = "monitorCenter",
	MousePosition = "mousePosition",
}

export enum TrayIconClickAction {
	ShowMainWindow = "showMainWindow",
	Screenshot = "screenshot",
}

export enum CloudSaveUrlType {
	S3 = "s3",
}

export enum TrayIconDefaultIcon {
	Default = "default",
	Light = "light",
	Dark = "dark",
	SnowDefault = "snow-default",
	SnowLight = "snow-light",
	SnowDark = "snow-dark",
}

export enum CloudSaveUrlFormat {
	Origin = "origin",
	Markdown = "markdown",
}

export enum DoubleClickAction {
	Copy = "copy",
	Save = "save",
	FixedToScreen = "fixedToScreen",
	None = "none",
}

export enum ExtraToolList {
	None = 0,
	ScanQrcode = 1,
	VideoRecord = 2,
}

export type SelectRectPreset = {
	name: string;
	selectParams: {
		minX: number;
		minY: number;
		width: number;
		height: number;
		radius: number;
		shadowWidth: number;
		shadowColor: string;
		lockAspectRatio: boolean | undefined;
		lockDragAspectRatio: boolean | undefined;
	};
};

export enum AppSettingsTheme {
	Light = "light",
	Dark = "dark",
	System = "system",
}

export enum ColorPickerShowMode {
	Always = 0,
	BeyondSelectRect = 1,
	Never = 2,
}

export enum OcrModel {
	RapidOcrV4 = "RapidOcrV4",
	RapidOcrV5 = "RapidOcrV5",
	WeChatOcr = "WeChatOcr",
}

export enum KeyDisplayDirection {
	Horizontal = "horizontal",
	Vertical = "vertical",
}

export type AppSettingsData = {
	[AppSettingsGroup.Common]: {
		theme: AppSettingsTheme;
		/** 主色 */
		mainColor: string;
		/** 圆角 */
		borderRadius: number;
		/** 紧凑布局 */
		enableCompactLayout: boolean;
		language: AppSettingsLanguage;
		/** 浏览器语言，用于自动切换语言 */
		browserLanguage: string;
	};
	[AppSettingsGroup.ThemeSkin]: {
		/** 皮肤路径 */
		skinPath: string;
		/** 皮肤透明�?*/
		skinOpacity: number;
		/** 皮肤位置 */
		skinPosition: "top" | "bottom" | "left" | "right" | "center";
		/** 皮肤模糊�?*/
		skinBlur: number;
		/** 皮肤遮罩模糊�?*/
		skinMaskBlur: number;
		/** 皮肤遮罩透明�?*/
		skinMaskOpacity: number;
		/** 皮肤图片大小 */
		skinImageSize: "cover" | "contain" | "fill";
		/** 皮肤混合模式 */
		skinMixBlendMode:
			| "unset"
			| "multiply"
			| "screen"
			| "overlay"
			| "darken"
			| "lighten"
			| "color-dodge"
			| "color-burn"
			| "hard-light"
			| "soft-light"
			| "difference"
			| "exclusion"
			| "hue"
			| "saturation"
			| "color"
			| "luminosity";
		/** 自定�?CSS */
		customCss: string;
	};
	[AppSettingsGroup.Screenshot]: {
		/** 界面缩放比例 */
		uiScale: number;
		/** 截图快捷键提示窗口隐藏的项（key 列表�?*/
		hotKeyTipHiddenKeys: string[];
		/** 工具栏缩放比�?*/
		toolbarUiScale: number;
		/** 选区控件样式 */
		controlNode: AppSettingsControlNode;
		/** 颜色选择器模�?*/
		colorPickerShowMode: ColorPickerShowMode;
		/** 超出选区范围的元素透明�?*/
		beyondSelectRectElementOpacity: number;
		/** 选区遮罩颜色 */
		selectRectMaskColor: string;
		/** 快捷键提示透明�?*/
		hotKeyTipOpacity: number;
		/** 全屏辅助线颜�?*/
		fullScreenAuxiliaryLineColor: string;
		/** 显示器中心辅助线颜色 */
		monitorCenterAuxiliaryLineColor: string;
		/** 颜色选择器中心辅助线颜色 */
		colorPickerCenterAuxiliaryLineColor: string;
		/** 禁用动画 */
		disableAnimation: boolean;
		/** 隐藏工具栏工�?*/
		toolbarHiddenToolList: DrawState[];
	};
	[AppSettingsGroup.FixedContent]: {
		/** 边框颜色 */
		borderColor: string;
	};
	[AppSettingsGroup.CommonTrayIcon]: {
		/** 自定义托盘图�?*/
		iconPath: string;
		/** 自定义托盘图标（暗黑�?*/
		iconPathDark: string;
		/** 默认图标 */
		defaultIcons: TrayIconDefaultIcon;
		/** 默认图标（暗黑） */
		defaultIconsDark: TrayIconDefaultIcon;
		/** 启用托盘 */
		enableTrayIcon: boolean;
	};
	[AppSettingsGroup.FunctionDraw]: {
		/** 锁定绘制工具 */
		lockDrawTool: boolean;
		/** 启用更精细的大小控制 */
		enableSliderChangeWidth: boolean;
		/** 独立的工具样�?*/
		toolIndependentStyle: boolean;
		/** 禁用快速选择元素 */
		disableQuickSelectElementToolList: DrawState[];
	};
	[AppSettingsGroup.Cache]: {
		menuCollapsed: boolean;
		chatModel: string;
		chatModelEnableThinking: boolean;
		colorPickerColorFormatIndex: number;
		prevImageFormat: ImageFormat;
		prevSelectRect: ElementRect;
		enableMicrophone: boolean;
		/** 是否启用锁定绘制工具 */
		enableLockDrawTool: boolean;
		/** 序列号工具是否禁用箭�?*/
		disableArrowPicker: boolean;
		/** 截图选区圆角 */
		selectRectRadius: number;
		/** 截图选区阴影宽度 */
		selectRectShadowWidth: number;
		/** 截图选区阴影颜色 */
		selectRectShadowColor: string;
		// 记录上一次使用的矩形工具
		lastRectTool: DrawState;
		// 记录上一次使用的箭头工具
		lastArrowTool: DrawState;
		// 记录上一次使用的滤镜工具
		lastFilterTool: DrawState;
		// 记录上一次使用的额外工具
		lastExtraTool: ExtraToolList;
		// 记录上一次使用的绘制额外工具
		lastDrawExtraTool: DrawState;
		// 上一次水印内�?		lastWatermarkText: string;
		/** 延迟截图秒数 */
		delayScreenshotSeconds: number;
		/** 锁定手动选区时的宽高�?*/
		lockDragAspectRatio: number;
		/** Tab 键是否启用查找子元素 */
		enableTabFindChildrenElements: boolean;
		/** 是否拒绝安装内置字体（portable 首次运行取消后置 true�?*/
		fontInstallDeclined: boolean;
	};
	[AppSettingsGroup.DrawToolbarKeyEvent]: Record<
		DrawToolbarKeyEventKey,
		DrawToolbarKeyEventValue
	>;
	[AppSettingsGroup.CommonKeyEvent]: Record<
		CommonKeyEventKey,
		CommonKeyEventValue
	>;
	[AppSettingsGroup.AppFunction]: Record<AppFunction, AppFunctionConfig>;
	[AppSettingsGroup.Render]: {
		antialias: boolean;
	};
	[AppSettingsGroup.SystemCommon]: {
		autoStart: boolean;
		autoCheckVersion: boolean;
		runLog: boolean;
	};
	[AppSettingsGroup.SystemChat]: {
		maxTokens: number;
		temperature: number;
		thinkingBudgetTokens: number;
	};
	[AppSettingsGroup.SystemNetwork]: {
		enableProxy: boolean;
	};
	[AppSettingsGroup.FunctionChat]: {
		autoCreateNewSession: boolean;
		/** 关闭窗口时自动创建新会话 */
		autoCreateNewSessionOnCloseWindow: boolean;
		chatApiConfigList: ChatApiConfig[];
	};
	[AppSettingsGroup.FunctionOcr]: {
		/** 文本识别模型 */
		ocrModel: OcrModel;
		/** 启用 PaddlePaddle OCR（云�?API�?*/
		htmlVisionModel: string;
		/** 图片转为 HTML �?System 提示�?*/
		htmlVisionModelSystemPrompt: string;
		/** 图片转为 Markdown �?System 提示�?*/
		markdownVisionModelSystemPrompt: string;
	};
	[AppSettingsGroup.FunctionTranslation]: {
		/** 优化 AI 翻译的排�?*/
		optimizeAiTranslationLayout: boolean;
		translationSystemPrompt: string;
		translationApiConfigList: TranslationApiConfig[];
		sourceLanguage: string;
		targetLanguage: string;
		translationDomain: TranslationDomain;
		translationType: TranslationType | string;
		/** 云翻译引擎配�?*/
		cloudTranslationConfig: CloudTranslationConfig;
		/** 是否启用云端翻译 */
		enableCloudTranslation: boolean;
	};
	[AppSettingsGroup.FunctionTranslationCache]: {
		cacheSourceLanguage: string;
		cacheTargetLanguage: string;
		cacheTranslationDomain: TranslationDomain;
		cacheTranslationType: TranslationType | string;
	};
	[AppSettingsGroup.FunctionScreenshot]: {
		/** 选取窗口子元�?*/
		findChildrenElements: boolean;
		/** 截图快捷键提�?*/
		shortcutCanleTip: boolean;
		/** 复制后自动保存文�?*/
		autoSaveOnCopy: boolean;
		/** 快速保存文�?*/
		fastSave: boolean;
		/** 截取当前具有焦点的窗口时复制到剪贴板 */
		focusedWindowCopyToClipboard: boolean;
		/** 截取全屏时复制到剪贴�?*/
		fullScreenCopyToClipboard: boolean;
		/** 双击后执�?*/
		doubleClickAction: DoubleClickAction;
		/** 复制图片文件到剪贴板 */
		copyImageFileToClipboard: boolean;
		/** 保存到云�?*/
		saveToCloud: boolean;
		/** 云端链接格式 */
		cloudSaveUrlFormat: CloudSaveUrlFormat;
		/** 云端资源代理网址 */
		cloudProxyUrl: string;
		/** 云端保存协议 */
		cloudSaveUrlType: CloudSaveUrlType;
		/** S3 访问密钥 ID */
		s3AccessKeyId: string;
		/** S3 访问密钥 */
		s3SecretAccessKey: string;
		/** S3 区域 */
		s3Region: string;
		/** S3 端点 */
		s3Endpoint: string;
		/** S3 桶名 */
		s3BucketName: string;
		/** S3 路径前缀 */
		s3PathPrefix: string;
		/** S3 强制路径样式 */
		s3ForcePathStyle: boolean;
		/** 保存文件路径 */
		saveFileDirectory: string;
		/** 保存文件格式 */
		saveFileFormat: ImageFormat;
		/** OCR 后自动执�?*/
		ocrAfterAction: OcrDetectAfterAction;
		/** OCR 复制时复制文�?*/
		ocrCopyText: boolean;
		/** 使用 STranslate 处理截图翻译 */
		enableSTranslate: boolean;
		/** STranslate 外部调用端口 */
		sTranslatePort: number;
		/** 选区预设 */
		selectRectPresetList: SelectRectPreset[];
	};
	[AppSettingsGroup.FunctionOutput]: {
		/** 手动保存文件名格�?*/
		manualSaveFileNameFormat: string;
		/** 自动保存文件名格�?*/
		autoSaveFileNameFormat: string;
		/** 快速保存文件名格式 */
		fastSaveFileNameFormat: string;
		/** 截取当前具有焦点的窗口文件名格式 */
		focusedWindowFileNameFormat: string;
		/** 截取全屏文件名格�?*/
		fullScreenFileNameFormat: string;
		/** 视频录制文件名格�?*/
		videoRecordFileNameFormat: string;
		/** 上传到云端文件名格式 */
		uploadToCloudSaveUrlFormat: string;
	};
	[AppSettingsGroup.FunctionFixedContent]: {
		/** 以鼠标为中心缩放 */
		zoomWithMouse: boolean;
		/** 自动缩放窗口 */
		autoResizeWindow: boolean;
		/** 固定屏幕后自�?OCR */
		autoOcr: boolean;
		/** 固定截图后自动复制到剪贴�?*/
		autoCopyToClipboard: boolean;
		/** 窗口初始位置 */
		initialPosition: AppSettingsFixedContentInitialPosition;
	};
	[AppSettingsGroup.FunctionFullScreenDraw]: {
		/** 默认工具 */
		defaultTool: DrawState;
	};
	[AppSettingsGroup.FunctionVideoRecord]: {
		/** 录制画面中隐藏工具栏 */
		enableExcludeFromCapture: boolean;
		/** 视频录制保存路径 */
		saveDirectory: string;
		/** 帧率 */
		frameRate: number;
		/** GIF 帧率 */
		gifFrameRate: number;
		/** 麦克风设�?*/
		microphoneDeviceName: string;
		/** 硬件加�?*/
		hwaccel: boolean;
		/** 编码�?*/
		encoder: string;
		/** 编码器预�?*/
		encoderPreset: string;
		/** 视频最大尺�?*/
		videoMaxSize: VideoMaxSize;
		/** GIF 最大尺�?*/
		gifMaxSize: VideoMaxSize;
		/** 动图格式 */
		gifFormat: GifFormat;
		/** 按键显示字体大小 */
		keyDisplayFontSize: number;
		/** 按键显示背景�?*/
		keyDisplayBackgroundColor: string;
		/** 按键显示文字颜色 */
		keyDisplayTextColor: string;
		/** 按键显示持续时间（毫秒） */
		keyDisplayDuration: number;
		/** 按键显示合并时间（毫秒） */
		keyDisplayMergeDuration: number;
		/** 按键显示方向 */
		keyDisplayDirection: KeyDisplayDirection;
	};
	[AppSettingsGroup.SystemScreenshot]: {
		historyValidDuration: HistoryValidDuration;
		/** 记录截图历史 */
		recordCaptureHistory: boolean;
		/** 截图历史保存编辑结果 */
		historySaveEditResult: boolean;
		/** OCR 热启�?*/
		ocrHotStart: boolean;
		/** OCR 模型写入内存 */
		ocrModelWriteToMemory: boolean;
		ocrDetectAngle: boolean;
		/** 尝试使用 Bitmap 格式写入到剪贴板 */
		tryWriteBitmapImageToClipboard: boolean;
		/** 启用多显示器截图 */
		enableMultipleMonitor: boolean;
		/** 更正颜色滤镜 */
		correctColorFilter: boolean;
		/** 更正 HDR 颜色 */
		correctHdrColor: boolean;
		/** HDR 颜色转换算法 */
		correctHdrColorAlgorithm: HdrColorAlgorithm;
	};
	[AppSettingsGroup.SystemScrollScreenshot]: {
		tryRollback: boolean;
		minSide: number;
		maxSide: number;
		sampleRate: number;
		imageFeatureDescriptionLength: number;
		imageFeatureThreshold: number;
	};
	[AppSettingsGroup.FunctionTrayIcon]: {
		/** 托盘点击�?*/
		iconClickAction: TrayIconClickAction;
	};
	[AppSettingsGroup.SystemCore]: {
		/** 热加载页面数�?*/
		hotLoadPageCount: number;
	};
	[AppSettingsGroup.FunctionGlobalShortcut]: {
		/** 全屏窗口被聚焦时禁用全局快捷�?*/
		disableOnFocusedFullScreenWindow: boolean;
	};
};

export const CanHiddenToolSet: Set<DrawState> = new Set([
	DrawState.Select,
	DrawState.Ellipse,
	DrawState.Arrow,
	DrawState.Pen,
	DrawState.Text,
	DrawState.SerialNumber,
	DrawState.Blur,
	DrawState.BlurFreeDraw,
	DrawState.Watermark,
	DrawState.Highlight,
	DrawState.Eraser,
	DrawState.Redo,
	DrawState.Fixed,
	DrawState.OcrDetect,
	DrawState.OcrTranslate,
	DrawState.ScrollScreenshot,
]);
