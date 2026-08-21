package
{
    import flash.display.Bitmap;
    import flash.display.MovieClip;
    import flash.geom.ColorTransform;

    public class TargetMarkerNative extends MovieClip
    {
        [Embed(source="Assets/Enhanced/AimMarker_RedTriangle_Enhanced.png", mimeType="image/png")]
        private var RedTriangleAsset:Class;

        [Embed(source="Assets/Enhanced/AimMarker_SimpleWhite_Enhanced.png", mimeType="image/png")]
        private var SimpleWhiteAsset:Class;

        [Embed(source="Assets/Direct/AimMarker_WhiteRedFrame_Original.png", mimeType="image/png")]
        private var WhiteRedFrameAsset:Class;

        [Embed(source="Assets/Direct/AimMarker_RedWing_Original.png", mimeType="image/png")]
        private var RedWingAsset:Class;

        [Embed(source="Assets/Direct/AimMarker_CyanDoubleV_Original.png", mimeType="image/png")]
        private var CyanDoubleVAsset:Class;

        [Embed(source="Assets/Vanilla/AimMarker_ThreatSkull_Vanilla.png", mimeType="image/png")]
        private var ThreatSkullAsset:Class;

        private var marker:Bitmap;
        private var activeStyle:int = 6;

        // The red triangle stays compact; the other silhouettes are only
        // slightly larger so they remain readable without dominating the HUD.
        private static const kTriangleBaseScale:Number = 0.125;
        // Keep the original white badge size/layout from the stable marker
        // build; only the newer reference styles use their compact canvas.
        private static const kWhiteBaseScale:Number = 0.232;
        // These three assets are the supplied 1024px originals. Their
        // transparent margins are kept intact; per-style visible bounds below
        // provide one consistent registration point without altering artwork.
        private static const kReferenceBaseScale:Number = 0.060;
        private static const kSkullBaseScale:Number = 0.140;
        private static const kNearAnchorGap:Number = 13.333;
        private static const kFarAnchorGap:Number = 20.0;
        private static const kNearMarkerScale:Number = 1.2;
        private static const kFarMarkerScale:Number = 0.6875;

        public function TargetMarkerNative()
        {
            super();
            mouseEnabled = false;
            mouseChildren = false;

            marker = new Bitmap();
            marker.smoothing = true;
            marker.visible = false;
            marker.alpha = 0.96;
            addChild(marker);
            applyStyle(6);
        }

        public function setMarkerStyle(markerStyle:Number):void
        {
            var requestedStyle:int = int(markerStyle);
            if (requestedStyle < 1)
            {
                requestedStyle = 1;
            }
            else if (requestedStyle > 6)
            {
                requestedStyle = 6;
            }

            if (requestedStyle != activeStyle)
            {
                applyStyle(requestedStyle);
            }
        }

        // Native code passes actual Scaleform stage pixels.
        public function updateMarker(
            screenX:Number,
            screenY:Number,
            isVisible:Boolean,
            markerScale:Number = 1.0):void
        {
            markerScale = Math.max(kFarMarkerScale, Math.min(kNearMarkerScale, markerScale));
            var displayScale:Number = markerScale * getStyleBaseScale();
            marker.scaleX = displayScale;
            marker.scaleY = displayScale;

            var gapT:Number = (markerScale - kFarMarkerScale) /
                (kNearMarkerScale - kFarMarkerScale);
            gapT = Math.max(0.0, Math.min(1.0, gapT));
            var anchorGap:Number = kFarAnchorGap -
                ((kFarAnchorGap - kNearAnchorGap) * gapT);

            var visibleCenterX:Number = getVisibleCenterX();
            var visibleBottom:Number = getVisibleBottom();
            marker.x = screenX - (visibleCenterX * displayScale);
            marker.y = screenY - (visibleBottom * displayScale) - anchorGap;
            marker.visible = isVisible;
        }

        // Native code queries the real runtime stage size.
        public function GetStageWidth():Number
        {
            return stage != null ? stage.stageWidth : 0.0;
        }

        public function GetStageHeight():Number
        {
            return stage != null ? stage.stageHeight : 0.0;
        }

        private function getStyleBaseScale():Number
        {
            // Keep the supplied reference assets compact despite their larger
            // source canvas. The white generated icon uses the old 256px base.
            switch (activeStyle)
            {
                case 1:
                    return kTriangleBaseScale;
                case 2:
                    return kWhiteBaseScale;
                case 3:
                case 4:
                case 5:
                    return kReferenceBaseScale;
                case 6:
                    return kSkullBaseScale;
                default:
                    return kSkullBaseScale;
            }
        }

        private function getVisibleBottom():Number
        {
            switch (activeStyle)
            {
                case 1:
                    return 164.0;
                case 2:
                    return 225.0;
                case 3:
                    return 582.0;
                case 4:
                    return 632.0;
                case 5:
                    return 633.0;
                case 6:
                    return 256.0;
                default:
                    return 256.0;
            }
        }

        private function getVisibleCenterX():Number
        {
            switch (activeStyle)
            {
                case 1:
                    return 128.0;
                case 2:
                    return 127.0;
                case 3:
                case 4:
                    return 511.5;
                case 5:
                    return 510.5;
                case 6:
                    return 98.0;
                default:
                    return 98.0;
            }
        }

        private function applyStyle(style:int):void
        {
            var embedded:Bitmap;

            switch (style)
            {
                case 1:
                    embedded = new RedTriangleAsset();
                    break;
                case 2:
                    embedded = new SimpleWhiteAsset();
                    break;
                case 3:
                    embedded = new WhiteRedFrameAsset();
                    break;
                case 4:
                    embedded = new RedWingAsset();
                    break;
                case 5:
                    embedded = new CyanDoubleVAsset();
                    break;
                case 6:
                    embedded = new ThreatSkullAsset();
                    break;
                default:
                    embedded = new ThreatSkullAsset();
                    style = 6;
                    break;
            }

            if (marker != null && contains(marker))
            {
                removeChild(marker);
            }

            marker = embedded;
            marker.smoothing = true;
            marker.visible = false;
            marker.alpha = 0.96;
            if (style == 6)
            {
                marker.transform.colorTransform =
                    new ColorTransform(1.0, 0.0, 0.0, 1.0);
            }
            addChild(marker);
            activeStyle = style;
        }
    }
}
