function temp = recebe_temperatura(index)
    global SerESP
    flushinput(SerESP);
    
    index = max(0, min(index, 10)); 
    index = num2str(index);
   
    fprintf(SerESP, 'get temp:%s\n', index);
    
    ler = strtrim(fscanf(SerESP, '%s')); % Retorno do echo do ESP32

    ler = strtrim(fscanf(SerESP, '%s')); % Retorno do valor requisitado
 
    temp = str2double(ler);
    
%     Modularização da temperatura
    temp = (temp - 20)/25;
    
    flushoutput(SerESP);
end
