# Rally Cross no ROG Ally

Adaptação do módulo [Rally Cross de tyrannotorus](https://github.com/tyrannotorus/emulator-deadstation-1-recomp-rally-cross) para Bazzite x86-64. Mantém o renderizador de corrida do módulo e apresenta os quadros do Vulkan em uma janela SDL de tela cheia no Gamescope. A configuração testada compõe o jogo em 1024 × 768 e apresenta em 1920 × 1080 com a proporção original de 4:3. O autor indica 60 quadros de jogo por segundo para um jogador.

## Compilar com seu disco

Use a edição norte-americana de PlayStation, SCUS-94308. Coloque seu arquivo `RallyCross.chd` no diretório de instalação. O SHA-1 do cabeçalho deve ser `62c54f5e1f989126a1f034c39646a0336c6c8321`. Para converter as faixas BIN/CUE, execute `chdman createcd -i "Rally Cross (USA).cue" -o RallyCross.chd`. O pacote público contém apenas a adaptação e as instruções.

Em um ambiente de compilação Linux x86-64 com Git, CMake, Ninja, compiladores C/C++ (Clang e GCC), Python 3, SDL2 de desenvolvimento, cabeçalhos Vulkan, glslangValidator, zlib e chdman, execute `bash /caminho/do/port/build.sh` a partir do diretório de instalação. Os pinos das dependências vêm do projeto original. No Bazzite usamos um contêiner de compilação. Inicie `./jogar.sh`, ou adicione esse script como jogo não Steam usando sua pasta como diretório de trabalho.

## Controle do Ally

| Entrada | Ação na corrida |
| --- | --- |
| RT | Acelerar |
| LT | Frear ou derrapar |
| Analógico esquerdo ou direcional | Virar |
| A | Confirmar nos menus e acelerar |
| LB / RB | Reduzir / subir marcha |
| Clique no analógico esquerdo / direito | Balançar o carro para a esquerda / direita |
| Menu | Pausar |

O jogo original lê acelerador e freio como botões digitais. O lançador configura os gatilhos somente para este jogo em cada abertura. A memória de cartão fica em `userdata/recomp-data/rallycross/saves/`.

## Verificação no hardware

A imagem de disco passou pela conferência do SHA-1. O programa x86-64 abriu pelo atalho não Steam, o driver RADV do Ryzen Z1 Extreme apresentou a imagem em 1920 × 1080 e a captura no Ally mostrou o carro na pista. O runtime reconheceu o controle integrado e carregou o perfil de gatilhos. Esta adaptação usa a licença e os avisos do projeto original.
