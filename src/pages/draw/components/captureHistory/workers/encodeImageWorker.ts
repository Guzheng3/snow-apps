// This file is an ES module for Web Worker compatibility
export {};

self.onmessage = async (
	event: MessageEvent<{
		imageBuffer: Uint8ClampedArray;
		width: number;
		height: number;
	}>,
) => {
	const { imageBuffer, width, height } = event.data;

	if (typeof OffscreenCanvas === "undefined") {
		throw new Error("[encodeImageWorker] OffscreenCanvas is not supported");
	}

	const canvase = new OffscreenCanvas(width, height);
	const ctx = canvase.getContext("2d");
	if (!ctx) {
		throw new Error("[encodeImageWorker] Failed to get context");
	}

	ctx.putImageData(
		new ImageData(imageBuffer as ImageDataArray, width, height),
		0,
		0,
	);

	const image = await canvase.convertToBlob({ type: "image/png" });

	self.postMessage({
		image: await image.arrayBuffer(),
	});
};
