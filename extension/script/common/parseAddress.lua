return function (address, client)
    if address:sub(1,1) == '@' then
        return {
            protocol = 'unix',
            address = address:sub(2),
            client = client,
        }
    end
    local ipv4, port = address:match("(%d+%.%d+%.%d+%.%d+):(%d+)")
    if ipv4 then
        return {
            protocol = 'tcp',
            address = ipv4,
            port = tonumber(port),
            client = client,
        }
    end
    local ipv6, port = address:match("%[([%d:a-fA-F]+)%]:(%d+)")
    if ipv6 then
        return {
            protocol = 'tcp',
            address = ipv6,
            port = tonumber(port),
            client = client,
        }
    end
    error "Invalid address."
end
