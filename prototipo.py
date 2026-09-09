import cv2

CAMERA_INDEX = 0
RESIZE_WIDTH = 960
WIN_NAME = "Wallis - Human Detector"

# Relative size thresholds based on bounding-box area ratio
FAR_THRESHOLD = 0.03
MID_THRESHOLD = 0.10


def classify_size(box_area, frame_area):
    ratio = box_area / frame_area
    if ratio < FAR_THRESHOLD:
        return "LEJOS", ratio
    if ratio < MID_THRESHOLD:
        return "MEDIO", ratio
    return "CERCA", ratio


def main():
    cap = cv2.VideoCapture(CAMERA_INDEX)

    if not cap.isOpened():
        print("No se pudo abrir la camara.")
        return

    hog = cv2.HOGDescriptor()
    hog.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())

    print("Controles:")
    print("  q -> salir")
    print("  s -> guardar captura")

    screenshot_count = 0

    while True:
        ok, frame = cap.read()
        if not ok:
            print("No se pudo leer un frame de la camara.")
            break

        h0, w0 = frame.shape[:2]
        if w0 > RESIZE_WIDTH:
            scale = RESIZE_WIDTH / w0
            frame = cv2.resize(frame, (int(w0 * scale), int(h0 * scale)))

        frame_h, frame_w = frame.shape[:2]
        frame_area = frame_w * frame_h

        rects, weights = hog.detectMultiScale(
            frame,
            winStride=(8, 8),
            padding=(8, 8),
            scale=1.05
        )

        people_count = 0

        for (x, y, w, h), weight in zip(rects, weights):
            if weight < 0.3:
                continue

            people_count += 1
            box_area = w * h
            label_size, ratio = classify_size(box_area, frame_area)

            color = (0, 255, 0)
            if label_size == "MEDIO":
                color = (0, 255, 255)
            elif label_size == "CERCA":
                color = (0, 128, 255)

            cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)

            text_1 = f"Persona {people_count}"
            text_2 = f"Tamano: {label_size} | conf: {float(weight):.2f}"
            text_3 = f"Area relativa: {ratio:.3f}"

            cv2.putText(frame, text_1, (x, max(20, y - 30)), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
            cv2.putText(frame, text_2, (x, max(40, y - 10)), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
            cv2.putText(frame, text_3, (x, y + h + 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

        status = f"Personas detectadas: {people_count}"
        cv2.putText(frame, status, (15, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 255, 255), 2)
        cv2.putText(frame, "q: salir | s: screenshot", (15, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 2)

        cv2.imshow(WIN_NAME, frame)
        key = cv2.waitKey(1) & 0xFF

        if key == ord('q'):
            break
        elif key == ord('s'):
            filename = f"wallis_capture_{screenshot_count:02d}.png"
            cv2.imwrite(filename, frame)
            print(f"Captura guardada: {filename}")
            screenshot_count += 1

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()

