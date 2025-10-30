import * as TWEEN from "@tweenjs/tween.js";
import { isEqual } from "es-toolkit";

/**
 * 基于 TWEEN 的动画工具类
 */
export class TweenAnimation<T extends object> {
	private static tweenGroup: TWEEN.Group = new TWEEN.Group();

	private tween: TWEEN.Tween<T>;
	private currentObject: T;
	private targetObject: T;
	private easingFunction: typeof TWEEN.Easing.Quadratic.Out;
	private duration: number;
	private onUpdate: (object: T) => void;
	private animationFrameId: number | undefined;
	/**
	 * 初始化动画状态
	 * @param defaultObject 初始状态
	 * @param onUpdate 更新状态回调
	 */
	constructor(
		defaultObject: T,
		easingFunction: typeof TWEEN.Easing.Quadratic.Out,
		duration: number,
		onUpdate: (object: T) => void,
	) {
		this.currentObject = defaultObject;
		this.targetObject = defaultObject;
		this.easingFunction = easingFunction;
		this.duration = duration;
		this.onUpdate = onUpdate;

		// 在构造函数中创建一次 tween 对象，后续重用
		this.tween = new TWEEN.Tween(this.currentObject)
			.easing(this.easingFunction)
			.onUpdate(this.onUpdate)
			.onComplete(() => {
				TweenAnimation.tweenGroup.remove(this.tween);
			});
		TweenAnimation.tweenGroup.add(this.tween);
	}

	/**
	 * 更新动画状态
	 * @param object
	 */
	public update = (object: T, ignoreAnimation: boolean = false) => {
		if (isEqual(object, this.targetObject)) {
			return;
		}

		this.targetObject = object;

		TweenAnimation.tweenGroup.remove(this.tween);
		this.tween.stop();
		this.tween
			.to(this.targetObject, ignoreAnimation ? 0 : this.duration)
			.start(undefined, true);
		TweenAnimation.tweenGroup.add(this.tween);

		this.animationLoop();
	};

	private animationLoop = () => {
		this.animationFrameId = requestAnimationFrame(() => {
			TweenAnimation.tweenGroup.update();
			if (this.tween.isPlaying()) {
				this.animationFrameId = requestAnimationFrame(this.animationLoop);
			} else {
				this.animationFrameId = undefined;
			}
		});
	};

	// 销毁释放资源
	public dispose = () => {
		if (this.animationFrameId) {
			cancelAnimationFrame(this.animationFrameId);
			this.animationFrameId = undefined;
		}

		if (this.tween) {
			this.tween.stop();
			TweenAnimation.tweenGroup.remove(this.tween);
		}

		// @ts-expect-error - 清理引用
		this.currentObject = undefined;
		// @ts-expect-error - 清理引用
		this.targetObject = undefined;
		// @ts-expect-error - 清理引用
		this.onUpdate = undefined;
		// @ts-expect-error - 清理引用
		this.easingFunction = undefined;
		// @ts-expect-error - 清理引用
		this.duration = undefined;
		// @ts-expect-error - 清理引用
		this.tween = undefined;
	};

	public getTargetObject = () => {
		return this.targetObject;
	};

	public isDone = () => {
		return !this.tween.isPlaying();
	};
}
