require('simet.simet_utils')

local mac_address = get_mac_address()
local hash = get_device_hash()

function has_data(data)
    local formDataLength = 0

    for key, value in pairs(data) do
        formDataLength = formDataLength + 1
    end

    return formDataLength > 0
end

function is_personal_data_registered()
    return read_uci_section('personal_data', 'user_data')
end

function get_registered_personal_data()
    local user_data = {}

    user_data['zipCode'] = read_uci_option('personal_data', 'user_data', 'zipCode') 
    user_data['contractedBandwidth'] = read_uci_option('personal_data', 'user_data', 'contractedBandwidth') 
    user_data['userName'] = read_uci_option('personal_data', 'user_data', 'userName') 
    user_data['email'] = read_uci_option('personal_data', 'user_data', 'email') 
    user_data['cpf'] = read_uci_option('personal_data', 'user_data', 'cpf') 
    user_data['cnpj'] = read_uci_option('personal_data', 'user_data', 'cnpj') 
    user_data['bandwidthUnit'] = read_uci_option('personal_data', 'user_data', 'bandwidthUnit') 
    user_data['unknownBandwidth'] = read_uci_option('personal_data', 'user_data', 'unknownBandwidth') 

    return user_data
end


function has_personal_data(user_data)
    return user_data['hasPersonalData']
end

function validate_fields(user_data,errors)
    validate_zip_code(user_data['zipCode'],errors)
    validate_bandwidth(user_data['contractedBandwidth'], user_data['bandwidthUnit'], user_data['unknownBandwidth'],errors)
    validate_name(user_data['userName'],errors)
    validate_email(user_data['email'],errors)
    validate_CPF(user_data['cpf'],errors)
    validate_CNPJ(user_data['cnpj'],errors)

    return not has_data(errors)

end

function empty_user_data()

    local empty_data = {}

    empty_data['zipCode'] = '' 
    empty_data['contractedBandwidth'] = ''
    empty_data['userName'] = ''    
    empty_data['cpf'] = ''
    empty_data['cnpj'] = '' 
    empty_data['bandwidthUnit'] = ''
    empty_data['unknownBandwidth'] = ''

    return empty_data

end

function save_user_data(user_data)

    local current_cursor = current_config_cursor()
    local factory_cursor = factory_config_cursor()

    create_config_file('personal_data')

    local empty_data = empty_user_data()

    for key, value in pairs(empty_data) do
        write_uci_option_("personal_data", "user_data", key, value, current_cursor)
        write_uci_option_("personal_data", "user_data", key, value, factory_cursor)
    end

    for key, value in pairs(user_data) do
        write_uci_option_("personal_data", "user_data", key, value, current_cursor)
        write_uci_option_("personal_data", "user_data", key, value, factory_cursor)
    end

    commit_uci_config("personal_data", current_cursor)
    commit_uci_config("personal_data", factory_cursor)   
end

function send_personal_data(user_data)

    if is_online() and is_simet_box() then

        io.popen('lua /usr/lib/lua/simet/first_connection.lua &')
    end

end

function error_class(errors, key)
    return errors[key] and 'error' or '' 
end

function validate_zip_code(cep, errors)

    cep = cep or ''

    cep = string.gsub(cep, "[%-_]", "")
    cep = trim(cep)
    

    if  #cep == 0 or #cep ~= 8 or not cep:match("^[0-9]+$") then
        errors['zipCode'] = true
    end
end


function validate_CPF(cpf, errors)

    cpf = cpf or ''

    local valid = true

    cpf = string.gsub(cpf, "[%.%-_]", "")

    if #trim(cpf) == 0 then
        return
    end

    --Elimina CPFs invalidos conhecidos    
    if #cpf ~= 11 or cpf == "00000000000" or cpf == "11111111111" or cpf == "22222222222" or cpf == "33333333333" or cpf == "44444444444" or cpf == "55555555555" or cpf == "66666666666" or cpf == "77777777777" or cpf == "88888888888" or cpf == "99999999999" then
        valid = false
    end 

    --Valida 1o digito 
    add = 0    

    for i = 1, 9, 1 do     
        add = add + tonumber(cpf:sub(i,i)) * (11 - i)
    end


    rev = 11 - (add % 11)  
    if (rev == 10 or rev == 11) then
        rev = 0   
    end


    if rev ~= tonumber(cpf:sub(10,10)) then
        valid = false 
    end


    --Valida 2o digito 
    add = 0    

    for i = 1, 10, 1 do        
        add = add + tonumber(cpf:sub(i,i)) * (12 - i)  
    end

    rev = 11 - (add % 11)  

    if rev == 10 or rev == 11 then  
        rev = 0    
    end


    if rev ~= tonumber(cpf:sub(11,11)) then        
        valid = false    
    end

    if not valid then
        errors['cpf'] = true    
    end

end


function validate_CNPJ(cnpj, errors)  

    cnpj = cnpj or ''

    local valid = true
    
    cnpj = string.gsub(cnpj, "[%.%-_/]", "")

    if #trim(cnpj) == 0 then
        return
    end

     
    if #cnpj ~= 14 then
        valid = false
    end
 
    --Elimina CNPJs invalidos conhecidos
    if cnpj == "00000000000000" or 
        cnpj == "11111111111111" or 
        cnpj == "22222222222222" or 
        cnpj == "33333333333333" or 
        cnpj == "44444444444444" or 
        cnpj == "55555555555555" or 
        cnpj == "66666666666666" or 
        cnpj == "77777777777777" or 
        cnpj == "88888888888888" or 
        cnpj == "99999999999999" then
             valid = false
    end    


    --Valida DVs
    local  tamanho = #cnpj - 2
    local numeros = cnpj:sub(1,tamanho)
    local digitos = cnpj:sub(tamanho+1)

    local soma = 0
    local pos = tamanho - 7
    local resultado

    for i = tamanho - 1, 0, -1 do
        soma = soma + tonumber(numeros:sub(tamanho-i,tamanho-i)) * pos
        pos = pos - 1
        if pos < 2 then
            pos = 9
        end
    end

    resultado = (soma % 11 < 2) and 0 or (11 - soma % 11)


    if resultado ~= tonumber(digitos:sub(1,1)) then
        valid = false
    end


    tamanho = tamanho + 1
    numeros = cnpj:sub(1,tamanho)
    soma = 0
    pos = tamanho - 7
    for i = tamanho -1, 0, -1 do
        soma = soma + tonumber(numeros:sub(tamanho-i,tamanho-i)) * pos
        pos = pos - 1
        if pos < 2 then
            pos = 9
        end
    end

    resultado = (soma % 11 < 2) and 0 or (11 - soma % 11)

    if resultado ~= tonumber(digitos:sub(2,2)) then
        valid = false
    end
           
    if not valid then
        errors['cnpj'] = true    
    end

end


function validate_name(name, errors)
    name = name or ''
    if  (#trim(name) == 0 or #trim(name) > 256) then
       errors['userName'] = true
    end
end

function validate_bandwidth(bandwidth, unit, unknown, errors)

    if unknown then
        return
    end

    local valid = true

    bandwidth = trim(bandwidth)

    if tonumber(bandwidth) == nil or #bandwidth == 0 then
        valid = false
    else
        bandwidth = tonumber(bandwidth)

        if (unit == 'm') then
            bandwidth = bandwidth*1000
        end

        if bandwidth <= 0 or bandwidth > 1000000 then
            valid = false
        end

    end
    
    if not valid then
        errors['contractedBandwidth'] = true
    end
end

function validate_email(email, errors)
    email = email or ''
    email = trim(email)
    if not email:match("[A-Za-z0-9%.%%%+%-]+@[A-Za-z0-9%.%%%+%-%_]+%.%w%w%w?%w?") or #trim(email) > 70 then
        errors['email'] = true
    end
end



