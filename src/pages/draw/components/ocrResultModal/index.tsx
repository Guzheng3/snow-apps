import {
	CloseOutlined,
	CopyOutlined,
	ExportOutlined,
	LinkOutlined,
	MailOutlined,
	MobileOutlined,
	QqOutlined,
} from "@ant-design/icons";
import { useContext, useEffect, useState, type ReactNode } from "react";
import { AntdContext } from "@/contexts/antdContext";
import type { OcrDetectResult } from "@/types/commands/ocr";
import { writeTextToClipboard } from "@/utils/clipboard";
import { openUrl } from "@tauri-apps/plugin-opener";
import styles from "./index.module.css";

type LayoutType = "original" | "semantic";

type ExtractedLinks = {
	urls: string[];
	emails: string[];
	phones: string[];
	qqs: string[];
};

/**
 * 原图格式排版：按识别顺序，每个文本块一行
 */
const originalLayout = (result: OcrDetectResult): string => {
	return result.text_blocks.map((block) => block.text).join("\n");
};

/**
 * 语义智能排版：按文本块几何位置聚类成行与段落
 * - 同一水平线的文本块合并为一行（按 x 排序，空格连接）
 * - 行间距较大的行之间用空行分隔，形成段落
 */
const semanticLayout = (result: OcrDetectResult): string => {
	const blocks = result.text_blocks;
	if (blocks.length === 0) {
		return "";
	}

	type LineItem = {
		text: string;
		cx: number;
		cy: number;
		minY: number;
		maxY: number;
		height: number;
	};

	const items: LineItem[] = blocks.map((block) => {
		const ys = block.box_points.map((p) => p.y);
		const xs = block.box_points.map((p) => p.x);
		return {
			text: block.text,
			cx: xs.reduce((a, c) => a + c, 0) / xs.length,
			cy: ys.reduce((a, c) => a + c, 0) / ys.length,
			minY: Math.min(...ys),
			maxY: Math.max(...ys),
			height: Math.max(...ys) - Math.min(...ys),
		};
	});

	// 按中心 y 排序
	items.sort((a, b) => a.cy - b.cy);

	// 聚类成行：与上一行中心 y 差距小于行高 → 同一行
	const lines: LineItem[][] = [];
	for (const item of items) {
		const lastLine = lines[lines.length - 1];
		if (lastLine && lastLine.length > 0) {
			const lastItem = lastLine[lastLine.length - 1];
			const avgHeight = lastItem.height || 1;
			if (item.cy - lastItem.cy < avgHeight * 0.8) {
				lastLine.push(item);
				continue;
			}
		}
		lines.push([item]);
	}

	// 每行内按 x 排序，拼接行文本
	const lineTexts = lines.map((line) => {
		line.sort((a, b) => a.cx - b.cx);
		return line.map((item) => item.text).join(" ");
	});

	// 段落：相邻行 y 间隙大于 1.5 倍行高 → 换段落（空行分隔）
	const paragraphs: string[][] = [];
	let prevBottom = -Infinity;
	for (let i = 0; i < lines.length; i++) {
		const line = lines[i];
		const lineTop = Math.min(...line.map((it) => it.minY));
		const lineBottom = Math.max(...line.map((it) => it.maxY));
		const avgHeight =
			line.reduce((a, it) => a + it.height, 0) / line.length || 1;
		if (i > 0 && lineTop - prevBottom > avgHeight * 1.5) {
			paragraphs.push([]);
		}
		if (paragraphs.length === 0) {
			paragraphs.push([]);
		}
		paragraphs[paragraphs.length - 1].push(lineTexts[i]);
		prevBottom = lineBottom;
	}

	// 语义合并：段落内，行尾无句末标点且下一行非段落开头 → 合并为一句（去掉换行）
	// 中文直接拼接；相邻英文/数字用空格分隔
	const sentenceEndPattern = /[。！？!?…；;：:"“”''）)】》」』]$/;
	const paragraphStartPattern =
		/^[（(【\[《“"「『]|^[0-9一二三四五六七八九十]+[、.．]|^[A-Za-z0-9#*•·-]/;

	const mergeParagraph = (lines: string[]): string => {
		const merged: string[] = [];
		for (const line of lines) {
			const trimmed = line.trim();
			if (!trimmed) {
				continue;
			}
			if (merged.length === 0) {
				merged.push(trimmed);
				continue;
			}
			const prev = merged[merged.length - 1];
			const prevEndsSentence = sentenceEndPattern.test(prev);
			const currStartsParagraph = paragraphStartPattern.test(trimmed);
			if (!prevEndsSentence && !currStartsParagraph) {
				// 合并为一句（去掉换行）
				const prevLastChar = prev[prev.length - 1];
				const currFirstChar = trimmed[0];
				const needSpace =
					/[A-Za-z0-9]/.test(prevLastChar) &&
					/[A-Za-z0-9]/.test(currFirstChar);
				merged[merged.length - 1] =
					prev + (needSpace ? " " : "") + trimmed;
			} else {
				merged.push(trimmed);
			}
		}
		return merged.join("\n");
	};

	return paragraphs.map(mergeParagraph).join("\n\n");
};

/**
 * 从文本中提取链接与邮箱（去重、去尾部标点）
 */
const extractLinks = (text: string): ExtractedLinks => {
	const urlSet = new Set<string>();
	const emailSet = new Set<string>();

	// URL：http(s):// 或 www. 开头，直到空白/引号/尖括号
	const urlPattern = /(?:https?:\/\/|www\.)[^\s<>"'“”‘’]+/gi;
	let m: RegExpExecArray | null;
	let cleaned = text;
	while ((m = urlPattern.exec(text)) !== null) {
		let url = m[0];
		// 去掉尾部常见标点（. , ; : ！？)】]）等）
		url = url.replace(/[.,;:!?。，；：！？）)】】》》」』"'“”‘’]+$/, "");
		if (url) {
			urlSet.add(url);
		}
		// 挖掉 URL（含认证段），避免其内部被误当邮箱
		cleaned = cleaned.replace(m[0], " ");
	}

	// 邮箱（在挖掉 URL 后的文本中提取）
	const emailPattern = /[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}/g;
	while ((m = emailPattern.exec(cleaned)) !== null) {
		emailSet.add(m[0]);
		// 挖掉邮箱，避免其内部被误当手机号/QQ
		cleaned = cleaned.replace(m[0], " ");
	}

	const phoneSet = new Set<string>();
	const qqSet = new Set<string>();

	// 手机号：1[3-9] 开头 11 位（中国大陆）
	const phonePattern = /(?<![0-9])1[3-9][0-9]{9}(?![0-9])/g;
	while ((m = phonePattern.exec(cleaned)) !== null) {
		phoneSet.add(m[0]);
		cleaned = cleaned.replace(m[0], " ");
	}

	// QQ 号：5-11 位独立数字段（排除手机号、排除 0 开头、排除长数字内截取）
	const qqPattern = /(?<![0-9])([1-9][0-9]{4,10})(?![0-9])/g;
	while ((m = qqPattern.exec(cleaned)) !== null) {
		const num = m[1];
		// 11 位且 1[3-9] 开头 = 手机号，跳过
		if (num.length === 11 && /^1[3-9]/.test(num)) continue;
		qqSet.add(num);
	}

	return { urls: [...urlSet], emails: [...emailSet], phones: [...phoneSet], qqs: [...qqSet] };
};

/**
 * 打开链接（www. 开头补 https://）
 */
const openLink = (url: string) => {
	const normalized = /^https?:\/\//i.test(url) ? url : `https://${url}`;
	openUrl(normalized);
};

const ICONS: Record<
	"urls" | "emails" | "phones" | "qqs",
	{ icon: ReactNode; label: string; type: "链接" | "邮箱" | "手机号" | "QQ 号" }
> = {
	urls: { icon: <LinkOutlined />, label: "链接", type: "链接" },
	emails: { icon: <MailOutlined />, label: "邮箱", type: "邮箱" },
	phones: { icon: <MobileOutlined />, label: "手机", type: "手机号" },
	qqs: { icon: <QqOutlined />, label: "QQ", type: "QQ 号" },
};

const ITEM_ORDER: ("urls" | "emails" | "phones" | "qqs")[] = [
	"urls",
	"emails",
	"phones",
	"qqs",
];

export const OcrResultModal: React.FC<{
	open: boolean;
	ocrResult: OcrDetectResult | undefined;
	onClose: () => void;
}> = ({ open, ocrResult, onClose }) => {
	const { message } = useContext(AntdContext);
	const [layoutType, setLayoutType] = useState<LayoutType>("original");
	const [editableText, setEditableText] = useState("");
	const [copying, setCopying] = useState(false);
	const [extracted, setExtracted] = useState<ExtractedLinks>({
		urls: [],
		emails: [],
		phones: [],
		qqs: [],
	});
	const [copiedItem, setCopiedItem] = useState("");

	// 每次 OCR 结果变化时，重置为语义智能排版（按几何位置聚类行与段落，更贴近阅读顺序）
	useEffect(() => {
		if (open && ocrResult) {
			setLayoutType("semantic");
			setEditableText(semanticLayout(ocrResult));
		}
	}, [open, ocrResult]);

	// 编辑内容变化 → 实时重新提取链接/邮箱（编辑后成为链接也会自动显示）
	useEffect(() => {
		setExtracted(extractLinks(editableText));
		setCopiedItem("");
	}, [editableText]);

	const handleLayoutChange = (type: LayoutType) => {
		setLayoutType(type);
		if (ocrResult) {
			setEditableText(
				type === "original"
					? originalLayout(ocrResult)
					: semanticLayout(ocrResult),
			);
		}
	};

	const handleCopy = async () => {
		if (!editableText) {
			return;
		}
		setCopying(true);
		try {
			await writeTextToClipboard(editableText);
			message.success("已复制到剪贴板");
		} catch {
			message.error("复制失败");
		} finally {
			setCopying(false);
		}
	};

	const handleCopyItem = async (
		value: string,
		type: "链接" | "邮箱" | "手机号" | "QQ 号",
	) => {
		try {
			await writeTextToClipboard(value);
			setCopiedItem(value);
			message.success(`${type}已复制`);
		} catch {
			message.error("复制失败");
		}
	};

	const handleItemClick = (
		key: "urls" | "emails" | "phones" | "qqs",
		value: string,
	) => {
		if (key === "urls") {
			openLink(value);
		} else {
			handleCopyItem(value, ICONS[key].type);
		}
	};

	if (!open) {
		return null;
	}

	const blockCount = ocrResult?.text_blocks.length ?? 0;
	const totalExtracted =
		extracted.urls.length +
		extracted.emails.length +
		extracted.phones.length +
		extracted.qqs.length;

	return (
		<div className={styles.panel}>
			{/* 顶部工具栏（可拖动区域） */}
			<div className={styles.header} data-tauri-drag-region>
				<div className={styles.titleWrap}>
					<h2 className={styles.title}>
						文本识别结果
						{blockCount > 0 && (
							<span className={styles.titleBadge}>
								{blockCount} 个文本块
							</span>
						)}
					</h2>
				</div>
				<div className={styles.headerActions}>
					<div className={styles.layoutGroup}>
						<button
							className={`${styles.layoutItem} ${
								layoutType === "original" ? styles.layoutItemActive : ""
							}`}
							onClick={() => handleLayoutChange("original")}
						>
							原图排版
						</button>
						<button
							className={`${styles.layoutItem} ${
								layoutType === "semantic" ? styles.layoutItemActive : ""
							}`}
							onClick={() => handleLayoutChange("semantic")}
						>
							语义排版
						</button>
					</div>
					<button
						className={styles.closeBtn}
						title="关闭"
						onClick={onClose}
					>
						<CloseOutlined />
					</button>
				</div>
			</div>

			{/* 编辑区 */}
			{blockCount === 0 ? (
				<div className={styles.empty}>识别结果为空</div>
			) : (
				<div className={styles.editor}>
					<div className={styles.editorBar}>
						<span className={styles.editorDot} />
						<span className={styles.editorHint}>可编辑</span>
					</div>
					<textarea
						className={styles.editorArea}
						value={editableText}
						onChange={(e) => setEditableText(e.target.value)}
						placeholder="识别结果为空"
						spellCheck={false}
					/>
				</div>
			)}

			{/* 提取区 */}
			{totalExtracted > 0 && (
				<div className={styles.extracted}>
					<div className={styles.extractedHeader}>
						<span className={styles.extractedTitle}>识别到的信息</span>
					</div>
					<div className={styles.extractedGrid}>
						{ITEM_ORDER.map((key) =>
							extracted[key].map((value) => {
								const meta = ICONS[key];
								const copied = copiedItem === value;
								return (
									<div
										key={`${key}-${value}`}
										className={styles.extractedItem}
									>
										<div
											className={styles.extractedMain}
											title={
												key === "urls"
													? "点击在浏览器打开"
													: "点击复制"
											}
											onClick={() => handleItemClick(key, value)}
										>
											<span className={styles.extractedIcon}>
												{meta.icon}
											</span>
											<span className={styles.extractedLabel}>
												{meta.label}
											</span>
											<span
												className={`${styles.extractedValue} ${
													key === "urls" ? styles.link : ""
												} ${copied ? styles.copied : ""}`}
											>
												{copied ? "已复制" : value}
											</span>
										</div>
										<div className={styles.extractedActions}>
											{key === "urls" && (
												<button
													className={styles.extractedAction}
													title="在浏览器打开"
													onClick={() => handleItemClick(key, value)}
												>
													<ExportOutlined />
												</button>
											)}
											<button
												className={styles.extractedAction}
												title="复制"
												onClick={() =>
													handleCopyItem(value, meta.type)
												}
											>
												<CopyOutlined />
											</button>
										</div>
									</div>
								);
							}),
						)}
					</div>
				</div>
			)}

			{/* 底部操作栏 */}
			<div className={styles.footer}>
				<span className={styles.footerHint}>
					{layoutType === "semantic"
						? "语义排版 · 已按阅读顺序整理"
						: "原图排版 · 与截图顺序一致"}
				</span>
				<div className={styles.footerRight}>
					<button
						className={styles.closeWinBtn}
						onClick={onClose}
					>
						<CloseOutlined />
						<span>关闭窗口</span>
					</button>
					<button
						className={styles.copyBtn}
						onClick={handleCopy}
						disabled={!editableText || copying}
					>
						<CopyOutlined />
						<span>{copying ? "复制中…" : "一键复制"}</span>
					</button>
				</div>
			</div>
		</div>
	);
};

export default OcrResultModal;
