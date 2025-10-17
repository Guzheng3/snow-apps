import * as TWEEN from "@tweenjs/tween.js";
import { isEqual } from "es-toolkit";

/**
 * 基于 TWEEN 的动画工具类
 */
export class TweenAnimation<T extends object> {
	private static tweenGroup: TWEEN.Group = new TWEEN.Group();

	private tween: TWEEN.Tween | undefined;
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

		// 如果存在动画，则停止并移除
		if (this.tween) {
			this.tween.stop();
			TweenAnimation.tweenGroup.remove(this.tween);
		}

		if (ignoreAnimation) {
			this.currentObject = this.targetObject;
			this.onUpdate(this.currentObject);
			return;
		}

		this.tween = new TWEEN.Tween(this.currentObject)
			.to(this.targetObject, this.duration)
			.easing(this.easingFunction)
			.onUpdate(this.onUpdate)
			.onComplete(() => {
				if (!this.tween) {
					return;
				}

				TweenAnimation.tweenGroup.remove(this.tween);
				this.tween = undefined;
			})
			.start();
		TweenAnimation.tweenGroup.add(this.tween);

		this.animationLoop();
	};

	private animationLoop = () => {
		this.animationFrameId = requestAnimationFrame(() => {
			TweenAnimation.tweenGroup.update();
			if (this.tween) {
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
			this.tween = undefined;
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
	};

	public getTargetObject = () => {
		return this.targetObject;
	};

	public isDone = () => {
		return this.tween === undefined;
	};
}
