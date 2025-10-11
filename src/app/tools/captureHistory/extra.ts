import { CaptureHistoryItem } from '@/utils/appStore';

export type CaptureHistoryRecordItem = CaptureHistoryItem & { file_path: string; file_url: string };
