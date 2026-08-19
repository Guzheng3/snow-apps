import { Button, Input, Modal, Radio, Space, Typography } from "antd";
import { useContext, useEffect, useState } from "react";
import { CopyOutlined } from "@ant-design/icons";
import { AntdContext } from "@/contexts/antdContext";
import type { OcrDetectResult } from "@/types/commands/ocr";
import { writeTextToClipboard } from "@/utils/clipboard";

type LayoutType = "original" | "semantic";

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
	const sentenceEndPattern = /[。！？!?…；;：:""''）)】》」』]$/;
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

export const OcrResultModal: React.FC<{
	open: boolean;
	ocrResult: OcrDetectResult | undefined;
	onClose: () => void;
}> = ({ open, ocrResult, onClose }) => {
	const { message } = useContext(AntdContext);
	const [layoutType, setLayoutType] = useState<LayoutType>("original");
	const [editableText, setEditableText] = useState("");
	const [copying, setCopying] = useState(false);

	// 每次 OCR 结果变化时，重置为原图格式排版
	useEffect(() => {
		if (open && ocrResult) {
			setLayoutType("original");
			setEditableText(originalLayout(ocrResult));
		}
	}, [open, ocrResult]);

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

	return (
		<Modal
			title="文本识别结果"
			open={open}
			onCancel={onClose}
			width={560}
			destroyOnClose
			footer={
				<Space>
					<Button onClick={onClose}>关闭</Button>
					<Button
						type="primary"
						icon={<CopyOutlined />}
						loading={copying}
						onClick={handleCopy}
					>
						一键复制
					</Button>
				</Space>
			}
		>
			<Space direction="vertical" style={{ width: "100%" }} size={12}>
				<Radio.Group
					value={layoutType}
					onChange={(e) => handleLayoutChange(e.target.value)}
					optionType="button"
					buttonStyle="solid"
				>
					<Radio.Button value="original">原图格式排版</Radio.Button>
					<Radio.Button value="semantic">语义智能排版</Radio.Button>
				</Radio.Group>
				<Typography.Text type="secondary" style={{ fontSize: 12 }}>
					识别结果可直接编辑，编辑后点击「一键复制」复制当前内容。
				</Typography.Text>
				<Input.TextArea
					value={editableText}
					onChange={(e) => setEditableText(e.target.value)}
					autoSize={{ minRows: 8, maxRows: 20 }}
					placeholder="识别结果为空"
					style={{ fontSize: 13 }}
				/>
			</Space>
		</Modal>
	);
};

export default OcrResultModal;
