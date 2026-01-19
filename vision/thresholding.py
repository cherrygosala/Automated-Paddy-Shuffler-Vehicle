import cv2

# CPS Stage: Continuous-to-Discrete State Transition
# Input: Grayscale image frames (continuous pixel values 0-255)
# Output: Binary image (discrete states: 0 or 255)
# Significance: Thresholding abstracts intensity into binary classification
#               needed for ROI-based decision logic in cyber layer

cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Camera not opened")
    exit()

# Threshold value: 128 (midpoint)
# Adaptive thresholding or manual tuning can be applied later
THRESHOLD_VALUE = 128

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # Stage 1: Convert to grayscale (dimensionality reduction)
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # Stage 2: Apply binary thresholding
    # Continuous grayscale values → Discrete binary states (0 or 255)
    _, binary = cv2.threshold(gray, THRESHOLD_VALUE, 255, cv2.THRESH_BINARY)

    # Display all three stages for verification
    cv2.imshow("Original (BGR)", frame)
    cv2.imshow("Grayscale", gray)
    cv2.imshow("Binary Thresholded", binary)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()