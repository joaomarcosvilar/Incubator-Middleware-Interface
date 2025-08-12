global SerESP
    flushoutput(SerESP);
    fclose(SerESP); %--close the serial port when done
    delete(SerESP);
    delete(instrfind);
    %clear all;
    clc;
