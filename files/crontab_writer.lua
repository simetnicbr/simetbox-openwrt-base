require("uci")


function generate_time_sequence(step, max_value)
    
    local sequence = ''

    if step == 1 then
        sequence = '*'
    else
        local first_value = math.random(0,step-1)
        for value = first_value,max_value-1,step do 
                sequence = sequence..value
                if value + step <= max_value-1 then 
                        sequence = sequence..','
                end
        end
    end

    return sequence
end

function add_crontab_entry(file, minute, time_step, command)
    time_step = tonumber(time_step)
    if validate_step(time_step) then
        if (time_step < 60) then
            local minutes = generate_time_sequence(time_step, 60)
            file:write(string.format("%s %s * * * %s\n" ,minutes, '*', command))
        else
            hour_step = time_step / 60
            local hours = generate_time_sequence(hour_step, 24)
            file:write(string.format("%d %s * * * %s\n" ,minute, hours, command))
        end

    end
end


function convert_to_minutes(number)
        return number % 60
end

function write_crontab(tests_table, config_table)
        local random_minute = math.random(0,59)

        file = io.open("/tmp/crontab_tmp", "w")

        local sum = 0

        for key,value in pairs(tests_table) do 
            if (value ~= nil) then
                    params = ''
                    if (config_table[key]['params'] ~= nil) then
                        for param_key,param_value in pairs(config_table[key]['params']) do 
                            params = params..' -'..param_key..' '..param_value
                        end
                    end
                    
                    command = tests_table[key].command..' '..params
                    add_crontab_entry(file, convert_to_minutes(random_minute + sum), config_table[key]['time_step'], command)
                    sum = sum + 7
            end
        end

        file:close()
        os.execute("cp /tmp/crontab_tmp /etc/crontabs/root")

end

function validate_positive_integer(number)
    return ( number ~= nil and (math.abs(number) == number) and (math.ceil(number) == number) and number > 0)
end

function validate_step(value)
    local number = tonumber(value)
    return validate_positive_integer(number) and 
    ( (number < 60 and 60 % number == 0 and number % 5 == 0) 
        or 
    ( number >= 60 and number % 60 == 0  and (24*60) % number == 0 ) )
        
end

function validate_param(value)
    local number = tonumber(value)
    return validate_positive_integer(number) and number < 100
end

function generate_crontab()
        

        local x = uci.cursor()

        local handle = io.popen("echo $(dd if=/dev/urandom bs=1024 count=1)| tr -cd '0-9' | head -c 5| sed 's/0*//'")

        local seed = handle:read("*a")

        handle:close()

        math.randomseed(seed) 

	local ntpq_exists = x:get("simet_cron","simet_ntpq", 'enable')      
                                                                                                                          
        local tests_table = {}                                                                                            
                                                                                                                          
        tests_table = {                                                
          auto_upgrade = { command = "auto_upgrade"},                
          simet_bcp38 = { command = "simet_bcp38"},                          
          content_provider = { command = "sendcontentprovider.sh -4 ; sendcontentprovider.sh -6"},          
          simet_dns = { command = "simet_dns; simet_dns_ping_traceroute.sh"},
          simet_ping = { params = {"c", "W"}, command = "simet_ping.sh" },
          simet_port25 = { command = "simet_porta25" } ,                     
          service_down = { command =  "sendifupdown.sh" },                                                      
          simet_alexa = { command = "simet_alexa" },                         
          simet_send_if_traffic = { command = "simet_send_if_traffic.sh" },
          simet_test = { command = "run_simet.sh"},
          simet_measure = { command = "simet-ma_run.sh --config /usr/lib/simet/simet-ma.conf --config /etc/simet/simet-ma.conf" }
        }                                                                                                         
                                                                                                                          
        local config_table = {}                                                                                           
                                                                                                                          
        local valid = true                                                                                                
                                                                                                                          
        for key,value in pairs(tests_table) do                                                                            
            config_table[key] = {}                                                                       
            config_table[key]['enable'] = x:get("simet_cron", key, 'enable')                             
            if config_table[key]['enable'] == 'true' then                                                
                config_table[key]['time_step'] =  x:get("simet_cron", key, 'time_step')                                      
                valid = valid and validate_step(config_table[key]['time_step'])                          
                if value.params ~= nil then                                                              
                    config_table[key]['params'] = {}                                                     
                    for param_key,param_value in pairs(value.params) do                                  
                        config_table[key]['params'][param_value] =  x:get("simet_cron", key, param_value)
                        valid = valid and validate_param(config_table[key]['params'][param_value])       
                    end                                                                                  
                end                                                                                      
            end                                                                                          
        end                

        if (valid) then
            write_crontab(tests_table, config_table)
        end

       
end
