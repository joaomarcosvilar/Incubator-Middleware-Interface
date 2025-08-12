function umid = recebe_umidade(index)
    global SerESP
    flushinput(SerESP);
    
    index = max(0, min(index, 1)); 
    index = num2str(index);
   
    fprintf(SerESP, 'get hum:%s\n', index);
    
    ler = strtrim(fscanf(SerESP, '%s')); % Retorno do echo do ESP32

    ler = strtrim(fscanf(SerESP, '%s')); % Retorno do valor requisitado
 
    umid = str2double(ler);
    
%     Modularização da umidade
%     umid = umid / 100;
    
    flushoutput(SerESP);
end
