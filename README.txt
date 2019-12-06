Sliding window protocol with "selective repeat"

Sender (send.c): Calculates the window size according to the formula, then divides the file size into packages.
Sends the first number of packages and packages that fit in the first window (all the file if it fits in one
window), then wait for a response from the Receiver that will send you NACK (id) -> resends the selected frame and keeps track that a it was necessary to send it again in the "toresend" variable. Otherwise, the Receiver will send the ACK (id), in which case we will check if it is the ack corresponding to the frame representing the beginning of the sliding window. If it is the expected ack we send the following frame, increasing the upperedge (id corresponding to the end of the sliding window), otherwise we read the following frame from the file and send it and we updated the expected ack id with the received id.
Thus we send the entire file storing it in frames, we do so to be able to send back the lost frames if needed.
After we finish sending the whole file, we wait to see if the program has received ack for the rest of the packages.
The sender will stop when all ack's have been received.

Receiver (recv.c): We test each received frame to see if it is the expected frame (ackexpected - the beginning of the sliding window).
We check if the id is corrupt, otherwise we check if the package is corrupt (checksum check), if it has corrupted, we send NACK (N), with
the respective id. If the received package is good, we store the frames in the frame buffer. We check if it is a frame corresponding to the window limits. If it is a frame inside the window we check if we have lost frames, if so we restart the counter because the ack / nacks for the losses have already been registered, otherwise we write in the file the frame.
The receiver will stop when all packets have been received.

Remarks: The verification of corruption is done by checksum (byte that stores the modulo 2 amount of all the bytes in the package)
The reordering and the loss will be realized with the help of "ackexpected" in the Receiver, who remembers which package (the corresponding id) will have to be received
and written to the file in the next step, so you will also notice the loss of a package.
It will also use the auxiliary function setFrame, which sets the data of a package (id, checksum and payload).