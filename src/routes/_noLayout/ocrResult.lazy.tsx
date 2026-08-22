import { createLazyFileRoute } from "@tanstack/react-router";
import { OcrResultPage } from "@/pages/ocrResult/page";

export const Route = createLazyFileRoute("/_noLayout/ocrResult")({
	component: RouteComponent,
});

function RouteComponent() {
	return <OcrResultPage />;
}
