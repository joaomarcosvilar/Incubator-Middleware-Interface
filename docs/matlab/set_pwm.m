function envia_potencia(percent)
global SerESP
    if percent > 100
        percent = 100;
    end
    if percent < 0
        percent = 0;
    end
    
    percent = num2str(percent);
    fprintf(SerESP, 'set pwm_hum:%s\n', percent);
end