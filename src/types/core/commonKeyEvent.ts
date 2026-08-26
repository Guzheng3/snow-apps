export enum CommonKeyEventGroup {
		Chat = "chat",
	FixedContent = "fixedContent",
}

export type CommonKeyEventValue = {
	hotKey: string;
	unique?: boolean;
	group: CommonKeyEventGroup;
};

export type CommonKeyEventComponentValue = CommonKeyEventValue & {
	messageId: string;
};

export enum CommonKeyEventKey {
	ChatCopyAndHide = "chatCopyAndHide",
	ChatCopy = "chatCopy",
	ChatNewSession = "chatNewSession",
	FixedContentEnableDraw = "fixedContentEnableDraw",
	FixedContentSwitchThumbnail = "fixedContentSwitchThumbnail",
	FixedContentAlwaysOnTop = "fixedContentAlwaysOnTop",
	FixedContentCloseWindow = "fixedContentCloseWindow",
	FixedContentCopyToClipboard = "fixedContentCopyToClipboard",
	// FixedContentCopyRawToClipboard = 'fixedContentCopyRawToClipboard',
	FixedContentSaveToFile = "fixedContentSaveToFile",
	FixedContentSelectText = "fixedContentSelectText",
	FixedContentSetOpacity = "fixedContentSetOpacity",
}
