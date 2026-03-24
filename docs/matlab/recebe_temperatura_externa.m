function temp = recebe_temperatura_externa()
    global SerESP
    flushinput(SerESP);
    
   
    fprintf(SerESP, 'get temp_ext:0\n');
    
%    ler = strtrim(fscanf(SerESP, '%s')); % Retorno do echo do ESP32

    ler = strtrim(fscanf(SerESP, '%s')); % Retorno do valor requisitado
 
    temp = str2double(ler);
    
%     Modularização da temperatura
%     temp = (temp - 20)/25;
    
    flushoutput(SerESP);
end
