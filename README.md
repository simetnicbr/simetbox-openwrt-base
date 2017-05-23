# SIMETBox Base Files

Estrutura básica para o SIMETBox

# Sobre o SIMETBox

O SIMETBox é um sistema inicialmente desenvolvido para roteadores com OpenWRT para medir a qualidade de vários quesitos na internet. Vários testes são realizados até os PTTs do [IX.br](http://ix.br). Os testes realizados pela solução são: latência até os PTTs e gateway da rede, perda de pacotes, jitter, vazão, qualidade dos sites mais acessados no Brasil, localização dos servidores de conteúdo, validação da [BCP-38](http://bcp.nic.br), teste para [gerência de porta 25](http://www.antispam.br/admin/porta25/definicao/) e testes de P2P. Os resultados ficam disponíveis aos usuários através de interface WEB e ao provedor através de [portal](https://pas.nic.br) próprio para isto.

## Instalação

```bash
git clone https://github.com/simetnicbr/simetbox-openwrt-base.git
cd simetbox-openwrt-base
autoreconf
automake --add-missing
./configure
make install

```

## Uso

É usado principalmente pelo projeto [simetbox-openwrt-feed](https://github.com/simetnicbr/simetbox-openwrt-feed).  
Descrições de uso para este pacote se encontram no pacote [simetbox-openwrt-base](https://github.com/simetnicbr/simetbox-openwrt-base).

## TODO

* Preparar pacote para instalação em Ubuntu e RedHat EL.

## Contribuições ao projeto

1. Crie um branch para a funcionalidade que foi desenvolvida: `git checkout -b <nova-funcionalidade>`
2. Envie sua alteração: `git commit -am '<descrição-da-funcionalidade>'`
3. Faça um push para o branch: `git push origin <nova-funcionalidade>`
4. Faça um pull request :D

## Histórico

2017-05-23 - Primeiro release público

## Créditos

NIC.br  
<medicoes@simet.nic.br>

## License

GNU Public Licence 2
