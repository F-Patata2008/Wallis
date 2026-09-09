#!/usr/bin/env python3
"""
Human detector con YOLOv8
- Detecta personas en webcam o video/imagen
- Dibuja bounding boxes con ID
- Calcula tamaño en píxeles y estimación de distancia
- Muestra FPS en tiempo real

Uso:
  python human_detector.py                   # webcam (índice 0)
  python human_detector.py --source video.mp4
  python human_detector.py --source imagen.jpg
  python human_detector.py --source 1        # segunda webcam
"""

import argparse
import sys
import time

import cv2
import numpy as np

# ---------------------------------------------------------------------------
# Intentar importar ultralytics, con mensaje claro si falta
# ---------------------------------------------------------------------------
try:
    from ultralytics import YOLO
except ImportError:
    print("[ERROR] Falta el paquete 'ultralytics'.")
    print("Instálalo con:  pip install ultralytics")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Constantes de estimación de distancia
# Para una cámara estándar a 1080p/720p, altura promedio de persona ≈ 1.70 m
# Fórmula: distancia = (altura_real * focal_length) / altura_px
# Focal length calibrado empíricamente (~700 px para webcam 720p).
# Ajusta FOCAL_PX según tu cámara si quieres más precisión.
# ---------------------------------------------------------------------------
ALTURA_PERSONA_M = 1.70   # metros
FOCAL_PX        = 700     # píxeles (focal length estimado)


def estimar_distancia(altura_px: int) -> float | None:
    """Devuelve distancia estimada en metros, o None si altura_px == 0."""
    if altura_px <= 0:
        return None
    return round((ALTURA_PERSONA_M * FOCAL_PX) / altura_px, 2)


def dibujar_box(frame, box, idx: int, conf: float):
    """Dibuja bounding box, etiqueta y métricas sobre el frame."""
    x1, y1, x2, y2 = map(int, box)

    ancho_px = x2 - x1
    alto_px  = y2 - y1
    area_px  = ancho_px * alto_px
    distancia = estimar_distancia(alto_px)

    # Color por persona (ciclo de paleta)
    colores = [
        (0, 255, 120), (0, 180, 255), (255, 100, 0),
        (180, 0, 255), (255, 220, 0), (0, 255, 220),
    ]
    color = colores[idx % len(colores)]

    # Bounding box
    cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)

    # Etiqueta principal
    label_main = f"#{idx}  {conf:.0%}"
    # Métricas secundarias
    label_size = f"{ancho_px}x{alto_px}px  area={area_px:,}"
    label_dist = f"~{distancia} m" if distancia else ""

    # Fondo semitransparente para texto
    margin = 4
    line_h = 18
    lines  = [label_main, label_size]
    if label_dist:
        lines.append(label_dist)

    box_w  = max(cv2.getTextSize(l, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)[0][0] for l in lines) + margin * 2
    box_h  = line_h * len(lines) + margin * 2
    ty     = max(y1 - box_h - 4, 0)

    overlay = frame.copy()
    cv2.rectangle(overlay, (x1, ty), (x1 + box_w, ty + box_h), (20, 20, 20), -1)
    cv2.addWeighted(overlay, 0.6, frame, 0.4, 0, frame)

    for i, line in enumerate(lines):
        cv2.putText(
            frame, line,
            (x1 + margin, ty + margin + line_h * (i + 1) - 4),
            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1, cv2.LINE_AA
        )

    # Punto central
    cx, cy = (x1 + x2) // 2, (y1 + y2) // 2
    cv2.circle(frame, (cx, cy), 4, color, -1)


def run(source, conf_threshold: float, show_fps: bool):
    print(f"[INFO] Cargando modelo YOLOv8n...")
    model = YOLO("yolov8n.pt")   # se descarga automáticamente la primera vez (~6 MB)
    print(f"[INFO] Modelo listo. Fuente: {source}")
    print("[INFO] Presiona 'q' para salir, 's' para captura de pantalla.")

    # Abrir fuente (int para webcam, str para archivo)
    try:
        src = int(source)
    except ValueError:
        src = source

    cap = cv2.VideoCapture(src)
    if not cap.isOpened():
        print(f"[ERROR] No se pudo abrir la fuente: {source}")
        sys.exit(1)

    fps_t    = time.perf_counter()
    fps_val  = 0.0
    frame_n  = 0
    snapshot = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            print("[INFO] Fin de la fuente.")
            break

        # Inferencia: solo clase 0 = "person"
        results = model.predict(
            frame,
            classes=[0],
            conf=conf_threshold,
            verbose=False,
        )

        personas = 0
        if results and results[0].boxes is not None:
            boxes = results[0].boxes
            for i, box in enumerate(boxes):
                conf_val = float(box.conf[0])
                dibujar_box(frame, box.xyxy[0], i, conf_val)
                personas += 1

        # HUD superior
        now   = time.perf_counter()
        frame_n += 1
        if frame_n % 10 == 0:
            fps_val = 10 / (now - fps_t)
            fps_t   = now

        hud = f"Personas: {personas}   FPS: {fps_val:.1f}   conf>={conf_threshold:.0%}"
        cv2.rectangle(frame, (0, 0), (len(hud) * 9 + 12, 28), (20, 20, 20), -1)
        cv2.putText(frame, hud, (8, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (200, 200, 200), 1, cv2.LINE_AA)

        cv2.imshow("Human Detector  [q=salir  s=snapshot]", frame)
        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        elif key == ord("s"):
            fname = f"snapshot_{snapshot:04d}.jpg"
            cv2.imwrite(fname, frame)
            print(f"[INFO] Snapshot guardado: {fname}")
            snapshot += 1

    cap.release()
    cv2.destroyAllWindows()


def main():
    parser = argparse.ArgumentParser(description="Detector de humanos con YOLOv8")
    parser.add_argument(
        "--source", default="0",
        help="Fuente de video: índice de webcam (0,1,...), ruta a video o imagen"
    )
    parser.add_argument(
        "--conf", type=float, default=0.4,
        help="Umbral de confianza (0.0 – 1.0, default 0.4)"
    )
    parser.add_argument(
        "--no-fps", action="store_true",
        help="Ocultar contador FPS"
    )
    args = parser.parse_args()
    run(args.source, args.conf, not args.no_fps)


if __name__ == "__main__":
    main()
