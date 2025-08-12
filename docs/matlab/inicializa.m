function z = inicializa(porta)
global SerESP
disp('Inicializando Serial...');
SerESP = serial(porta); %<--change this appropriately
set(SerESP,'BaudRate', 115200, 'DataBits', 8, 'Parity', 'odd','StopBits', 1, 'FlowControl', 'none');
fopen(SerESP); %--open the serial port to the PIC
disp('--> pass 1');
pause(1)
disp('--> pass 2');
beep
pause(1)
disp('Serial ok');