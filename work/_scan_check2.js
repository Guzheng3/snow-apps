"use strict";
import { openUrl } from "@tauri-apps/plugin-opener";
import { Button, Spin, Typography, theme } from "antd";
import {
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState
} from "react";
import { useIntl } from "react-intl";
import { DrawStatePublisher } from "@/components/drawCore/extra";
import { AntdContext } from "@/contexts/antdContext";
import { useHotkeysApp } from "@/hooks/useHotkeysApp";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import { DrawContext } from "@/pages/draw/types";
import { DrawState } from "@/types/draw";
import { getPlatformValue } from "@/utils/platform";
import { zIndexs } from "@/utils/zIndex";
import { useMonitorRect } from "../../../statusBar";
const ScanQrcodeToolCore = () => {
  const intl = useIntl();
  const { token } = theme.useToken();
  const { message } = useContext(AntdContext);
  const { selectLayerActionRef, imageLayerActionRef, finishCapture } = useContext(DrawContext);
  const containerElementRef = useRef(null);
  const [containerStyle, setContainerStyle] = useState({});
  const [qrCode, setQrCode] = useState(void 0);
  const {
    contentScale: [contentScale]
  } = useMonitorRect();
  const init = useCallback(async () => {
    const selectRect = selectLayerActionRef.current?.getSelectRect();
    if (!selectRect) {
      return;
    }
    setContainerStyle({
      width: (selectRect.max_x - selectRect.min_x) / window.devicePixelRatio,
      height: (selectRect.max_y - selectRect.min_y) / window.devicePixelRatio,
      left: selectRect.min_x / window.devicePixelRatio,
      top: selectRect.min_y / window.devicePixelRatio,
      opacity: 1
    });
    const imageBitmap = await imageLayerActionRef.current?.getImageBitmap(selectRect);
    if (!imageBitmap) {
      return;
    }
    const tempCanvas = document.createElement("canvas");
    tempCanvas.width = selectRect.max_x - selectRect.min_x;
    tempCanvas.height = selectRect.max_y - selectRect.min_y;
    const tempCtx = tempCanvas.getContext("2d");
    if (!tempCtx) {
      return;
    }
    tempCtx.drawImage(imageBitmap, 0, 0);
    let QrCodeScanner;
    if (import.meta.env.PUBLIC_ONLINE_STATUS === "true") {
      QrCodeScanner = await import(
        // @ts-expect-error
        "https://snowshot.top/npm/qr-scanner-wechat/dist/index.mjs"
      );
    } else {
      QrCodeScanner = await import("qr-scanner-wechat");
    }
    try {
      await QrCodeScanner.ready();
      const result = await QrCodeScanner.scan(tempCanvas);
      setQrCode(result.text ?? "");
      if (!result.text) {
        message.warning(
          intl.formatMessage({
            id: "draw.extraTool.scanQrcode.error"
          })
        );
      }
    } catch (error) {
      console.error(error);
      message.warning(
        intl.formatMessage({
          id: "draw.extraTool.scanQrcode.error"
        })
      );
    }
  }, [selectLayerActionRef, imageLayerActionRef, message, intl]);
  const inited = useRef(false);
  useEffect(() => {
    if (inited.current) {
      return;
    }
    inited.current = true;
    init();
  }, [init]);
  useHotkeysApp(
    getPlatformValue("Ctrl+A", "Meta+A"),
    (event) => {
      event.preventDefault();
      const selection = window.getSelection();
      if (containerElementRef.current && selection) {
        const range = document.createRange();
        range.selectNodeContents(containerElementRef.current);
        selection.removeAllRanges();
        selection.addRange(range);
      }
    },
    {
      preventDefault: true,
      keyup: false,
      keydown: true
    }
  );
  const qrCodeContent = useMemo(() => {
    if (!qrCode) {
      return "";
    }
    if (qrCode.startsWith("http") || qrCode.startsWith("https")) {
      return /* @__PURE__ */ React.createElement(
        "a",
        {
          onClick: () => {
            openUrl(qrCode);
            finishCapture();
          }
        },
        qrCode
      );
    }
    return qrCode;
  }, [finishCapture, qrCode]);
  return /* @__PURE__ */ React.createElement(
    "div",
    {
      style: {
        opacity: 0,
        ...containerStyle,
        width: typeof containerStyle.width === "number" ? containerStyle.width / contentScale : 0,
        height: typeof containerStyle.height === "number" ? containerStyle.height / contentScale : 0,
        background: token.colorBgContainer,
        padding: token.padding,
        position: "fixed",
        zIndex: zIndexs.Draw_ScanQrcodeResult,
        pointerEvents: "auto",
        boxSizing: "border-box",
        transition: `opacity ${token.motionDurationFast} ${token.motionEaseInOut}`,
        transformOrigin: "top left",
        transform: `scale(${contentScale})`
      },
      ref: containerElementRef
    },
    qrCode === void 0 ? /* @__PURE__ */ React.createElement(Spin, { spinning: true }) : /* @__PURE__ */ React.createElement(
      Typography.Paragraph,
      {
        copyable: qrCode ? {
          text: qrCode,
          onCopy: () => {
            finishCapture();
          }
        } : false
      },
      qrCodeContent
    ),
    qrCode && (qrCode.startsWith("http") || qrCode.startsWith("https")) && /* @__PURE__ */ React.createElement(
      Button,
      {
        type: "primary",
        size: "small",
        onClick: () => {
          openUrl(qrCode);
          finishCapture();
        },
        style: { marginTop: 8 }
      },
      "\u7528\u9ED8\u8BA4\u6D4F\u89C8\u5668\u6253\u5F00"
    )
  );
};
export const ScanQrcodeTool = () => {
  const [enable, setEnable] = useState(false);
  useStateSubscriber(
    DrawStatePublisher,
    useCallback((drawState) => {
      setEnable(drawState === DrawState.ScanQrcode);
    }, [])
  );
  if (!enable) {
    return null;
  }
  return /* @__PURE__ */ React.createElement(ScanQrcodeToolCore, null);
};
