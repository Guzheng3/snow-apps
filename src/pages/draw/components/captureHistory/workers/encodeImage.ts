import { supportOffscreenCanvas } from "@/utils/environment";

export type DecodeResult = {
	data: ImageData;
	width: number;
	height: number;
};

const decodeWorker: Worker =
	typeof window !== "undefined" && supportOffscreenCanvas()
		? new Worker(new URL("./encodeImageWorker.ts", import.meta.url))
		: (undefined as unknown as Worker);

export async function encodeImage(
	width: number,
	height: number,
	imageBuffer: Uint8ClampedArray,
): Promise<ArrayBuffer | undefined> {
	if (!supportOffscreenCanvas()) {
		const canvase = new HTMLCanvasElement();
		canvase.width = width;
		canvase.height = height;
		const ctx = canvase.getContext("2d");
		if (!ctx) {
			return undefined;
		}

		ctx.putImageData(
			new ImageData(imageBuffer as ImageDataArray, width, height),
			0,
			0,
		);

		const image = new Promise<ArrayBuffer | undefined>((resolve) => {
			canvase.toBlob(
				async (blob) => {
					if (!blob) {
						resolve(undefined);
						return;
					}

					resolve(await blob.arrayBuffer());
				},
				"image/png",
				1,
			);
		});

		return image;
	}

	return new Promise((resolve, reject) => {
		decodeWorker.onmessage = async (
			event: MessageEvent<{
				image: ArrayBuffer;
			}>,
		) => {
			resolve(event.data.image);
		};

		decodeWorker.onerror = (error) => {
			reject(error);
		};

		decodeWorker.postMessage({ imageBuffer, width, height });
	});
}

export function getEncodeImageWorker() {
	return decodeWorker;
}
