import {
	AppSettingsControlNode,
	type AppSettingsData,
	AppSettingsFixedContentInitialPosition,
	AppSettingsGroup,
	AppSettingsLanguage,
	AppSettingsTheme,
	CloudSaveUrlFormat,
	CloudSaveUrlType,
	ColorPickerShowMode,
	ExtraToolList,
	GifFormat,
	HdrColorAlgorithm,
	HistoryValidDuration,
	OcrDetectAfterAction,
	OcrModel,
	TrayIconClickAction,
	TrayIconDefaultIcon,
	VideoMaxSize,
} from "@/types/appSettings";
import { DrawState } from "@/types/draw";
import {
	TranslationDomain,
	TranslationType,
} from "@/types/servies/translation";
import { ImageFormat } from "@/types/utils/file";
import { getPlatformValue } from "@/utils/platform";
import { defaultAppFunctionConfigs } from "./appFunction";
import { defaultCommonKeyEventSettings } from "./commonKeyEvent";
import { FOCUS_WINDOW_APP_NAME_ENV_VARIABLE } from "./components/chat";
import { defaultTranslationPrompt } from "./components/translation";
import { defaultDrawToolbarKeyEventSettings } from "./drawToolbarKeyEvent";

export const defaultAppSettingsData: AppSettingsData = {
	[AppSettingsGroup.Common]: {
		theme: AppSettingsTheme.System,
		enableCompactLayout: false,
		language: AppSettingsLanguage.ZHHans,
		browserLanguage: "",
	},
	[AppSettingsGroup.Screenshot]: {
		uiScale: 100,
		toolbarUiScale: 100,
		controlNode: AppSettingsControlNode.Circle,
		// 在 Mac 上禁用动画
		disableAnimation: getPlatformValue(false, true),
		colorPickerShowMode: ColorPickerShowMode.BeyondSelectRect,
		beyondSelectRectElementOpacity: 100,
		selectRectMaskColor: "#00000080",
		fullScreenAuxiliaryLineColor: "#00000000",
		monitorCenterAuxiliaryLineColor: "#00000000",
		hotKeyTipOpacity: 100,
		colorPickerCenterAuxiliaryLineColor: "#00000000",
		toolbarHiddenToolList: [],
	},
	[AppSettingsGroup.FixedContent]: {
		borderColor: "#dbdbdb",
	},
	[AppSettingsGroup.CommonTrayIcon]: {
		iconPath: "",
		defaultIcons: TrayIconDefaultIcon.Default,
		enableTrayIcon: true,
	},
	[AppSettingsGroup.FunctionDraw]: {
		lockDrawTool: true,
		enableSliderChangeWidth: false,
		toolIndependentStyle: true,
		disableQuickSelectElementToolList: [],
	},
	[AppSettingsGroup.Cache]: {
		menuCollapsed: false,
		chatModel: "deepseek-reasoner",
		translationType: TranslationType.Youdao,
		translationDomain: TranslationDomain.General,
		targetLanguage: "",
		ocrTranslateAutoReplace: true,
		ocrTranslateKeepLayout: false,
		ocrTranslateShowProcess: false,
		colorPickerColorFormatIndex: 0,
		prevImageFormat: ImageFormat.PNG,
		prevSelectRect: {
			min_x: 0,
			min_y: 0,
			max_x: 0,
			max_y: 0,
		},
		enableMicrophone: false,
		enableLockDrawTool: false,
		disableArrowPicker: true,
		selectRectRadius: 0,
		selectRectShadowWidth: 0,
		selectRectShadowColor: "#595959",
		lastRectTool: DrawState.Rect,
		lastArrowTool: DrawState.Arrow,
		lastFilterTool: DrawState.Blur,
		lastExtraTool: ExtraToolList.None,
		lastDrawExtraTool: DrawState.Idle,
		lastWatermarkText: "",
		delayScreenshotSeconds: 0,
		lockDragAspectRatio: 0,
	},
	[AppSettingsGroup.DrawToolbarKeyEvent]: defaultDrawToolbarKeyEventSettings,
	[AppSettingsGroup.CommonKeyEvent]: defaultCommonKeyEventSettings,
	[AppSettingsGroup.AppFunction]: defaultAppFunctionConfigs,
	[AppSettingsGroup.Render]: {
		antialias: true,
	},
	[AppSettingsGroup.SystemCommon]: {
		autoStart: true,
		autoCheckVersion: true,
		runLog: false,
	},
	[AppSettingsGroup.SystemChat]: {
		maxTokens: 4096,
		temperature: 1,
		thinkingBudgetTokens: 4096,
	},
	[AppSettingsGroup.SystemNetwork]: {
		enableProxy: false,
	},
	[AppSettingsGroup.FunctionChat]: {
		autoCreateNewSession: true,
		autoCreateNewSessionOnCloseWindow: true,
		chatApiConfigList: [],
	},
	[AppSettingsGroup.FunctionTranslation]: {
		chatPrompt: defaultTranslationPrompt,
		translationApiConfigList: [],
	},
	[AppSettingsGroup.FunctionScreenshot]: {
		findChildrenElements: true,
		shortcutCanleTip: true,
		autoSaveOnCopy: false,
		doubleClickCopyToClipboard: true,
		copyImageFileToClipboard: false,
		focusedWindowCopyToClipboard: true,
		fullScreenCopyToClipboard: true,
		fastSave: false,
		/** 保存到云端 */
		saveToCloud: false,
		/** 云端保存协议 */
		cloudSaveUrlType: CloudSaveUrlType.S3,
		cloudSaveUrlFormat: CloudSaveUrlFormat.Origin,
		s3AccessKeyId: "",
		s3SecretAccessKey: "",
		s3Region: "",
		s3Endpoint: "",
		s3BucketName: "",
		s3PathPrefix: "",
		s3ForcePathStyle: false,
		saveFileDirectory: "",
		saveFileFormat: ImageFormat.PNG,
		ocrAfterAction: OcrDetectAfterAction.None,
		ocrCopyText: true,
		selectRectPresetList: [],
	},
	[AppSettingsGroup.SystemScrollScreenshot]: {
		tryRollback: true,
		imageFeatureThreshold: 24,
		minSide: 128,
		maxSide: 128,
		sampleRate: 1,
		imageFeatureDescriptionLength: 28,
	},
	[AppSettingsGroup.FunctionFixedContent]: {
		zoomWithMouse: true,
		autoOcr: true,
		autoCopyToClipboard: false,
		initialPosition: AppSettingsFixedContentInitialPosition.MousePosition,
	},
	[AppSettingsGroup.FunctionOutput]: {
		manualSaveFileNameFormat: `SnowShot_{{YYYY-MM-DD_HH-mm-ss}}`,
		autoSaveFileNameFormat: `SnowShot_{{YYYY-MM-DD_HH-mm-ss}}`,
		fastSaveFileNameFormat: `SnowShot_{{YYYY-MM-DD_HH-mm-ss}}`,
		focusedWindowFileNameFormat: `${FOCUS_WINDOW_APP_NAME_ENV_VARIABLE}/SnowShot_{{YYYY-MM-DD_HH-mm-ss}}`,
		fullScreenFileNameFormat: `SnowShot_{{YYYY-MM-DD_HH-mm-ss}}`,
		videoRecordFileNameFormat: `SnowShot_Video_{{YYYY-MM-DD_HH-mm-ss}}`,
	},
	[AppSettingsGroup.FunctionFullScreenDraw]: {
		defaultTool: DrawState.Select,
	},
	[AppSettingsGroup.FunctionVideoRecord]: {
		enableExcludeFromCapture: true,
		saveDirectory: "",
		frameRate: 24,
		gifFrameRate: 10,
		microphoneDeviceName: "",
		hwaccel: true,
		encoder: "libx264",
		encoderPreset: "ultrafast",
		videoMaxSize: VideoMaxSize.P1080,
		gifMaxSize: VideoMaxSize.P1080,
		gifFormat: GifFormat.Gif,
	},
	[AppSettingsGroup.SystemScreenshot]: {
		ocrModel: OcrModel.RapidOcrV4,
		ocrHotStart: true,
		ocrModelWriteToMemory: false,
		ocrDetectAngle: false,
		historyValidDuration: HistoryValidDuration.Week,
		recordCaptureHistory: true,
		historySaveEditResult: true,
		enableBrowserClipboard: true,
		/** 尝试使用 Bitmap 格式写入到剪贴板 */
		tryWriteBitmapImageToClipboard: true,
		/** 启用多显示器截图 */
		enableMultipleMonitor: true,
		/** 更正颜色滤镜 */
		correctColorFilter: true,
		/** 更正 HDR 颜色  */
		correctHdrColor: true,
		/** HDR 颜色转换算法 */
		correctHdrColorAlgorithm: HdrColorAlgorithm.Linear,
	},
	[AppSettingsGroup.FunctionTrayIcon]: {
		iconClickAction: TrayIconClickAction.Screenshot,
	},
	[AppSettingsGroup.SystemCore]: {
		/// 热加载页面数量
		hotLoadPageCount: 2,
	},
};
