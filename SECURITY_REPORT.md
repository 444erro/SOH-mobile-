# Relatório de Segurança — SOH-Mobile-Anchor

Data da análise: 29/06/2026  
Escopo: fontes Android/Java/C/C++, configuração Gradle/CMake, multiplayer Anchor e APK de debug.

## Resumo geral

O projeto não continha chaves privadas, senhas, tokens de API, Firebase, WebView ou coleta de e-mail, telefone, localização e identificadores do aparelho. O APK atualizado solicita somente `INTERNET` e `VIBRATE` e contém as quatro arquiteturas configuradas.

Foram corrigidos riscos altos relacionados a assinatura de release com chave de debug, libpng vulnerável, strings de formato inseguras e consumo de memória por tráfego de rede. Backup de dados, logs sensíveis e o protocolo TCP binário legado também foram removidos/endurecidos. O acesso amplo ao armazenamento foi posteriormente restaurado por solicitação funcional explícita para permitir o uso manual de `/SOH/mods`; esse risco permanece aceito e deve ser revisto antes de publicação em loja.

Risco residual principal: o protocolo Anchor usa socket TCP nativo sem TLS, autenticação criptográfica ou integridade de mensagens. Corrigir isso somente no Android quebraria a compatibilidade; exige alteração coordenada no servidor Anchor e no cliente de PC.

## Vulnerabilidades encontradas e correções

### 1. Alto — acesso desnecessário a todos os arquivos (corrigido)

- Local anterior: `AndroidManifest.xml` e `MainActivity.java`.
- Risco: `MANAGE_EXTERNAL_STORAGE`, leitura e gravação externas davam ao app acesso muito maior que o necessário, incluindo dados compartilhados do usuário.
- Exploração em alto nível: código malicioso inserido no app, biblioteca comprometida ou falha futura poderia ler/copiar arquivos compartilhados.
- Estado atual: restaurado por solicitação explícita do projeto. O app pede `MANAGE_EXTERNAL_STORAGE` no Android 11+ e usa o diretório público `/SOH`, para que mods possam ser copiados manualmente para `/SOH/mods`.
- Mitigação aplicada: o código do aplicativo limita suas operações ao diretório `/SOH`, mantém fallback para o diretório protegido quando a permissão é negada e não envia esses arquivos pela rede. Ainda assim, a permissão concedida pelo Android é ampla.
- Compatibilidade: há migração automática da antiga pasta `/SOH` quando o Android ainda permite sua leitura. Em Android moderno, pode ser necessário copiar saves/mods manualmente. Depois de confirmar a migração, o usuário deve apagar a pasta `/SOH` antiga para não deixar saves expostos.

### 2. Alto — release assinada com certificado de debug (corrigido na configuração)

- Local anterior: `Android/app/build.gradle`, bloco `release`.
- Risco: o certificado de debug é público/inseguro por projeto e não deve identificar uma versão publicada.
- Correção: removido `signingConfig signingConfigs.debug` da release. Uma release agora precisa ser assinada com chave privada própria ou Play App Signing.
- Atenção: o APK atual em `build/outputs/apk/debug` continua sendo deliberadamente um APK de debug, está `debuggable=true` e usa `CN=Android Debug`. Ele serve apenas para testes e não deve ser publicado.

### 3. Alto — libpng 1.6.37 vulnerável ao processar PNG malformado (corrigido no código)

- Local: `ZAPDTR/ZAPD/CMakeLists.txt:348`.
- Risco: libpng anterior a 1.6.51 é afetada por leitura fora dos limites ao abrir PNG criado maliciosamente, com impacto de vazamento de memória ou encerramento do app (CVE-2025-64720).
- Correção: dependência atualizada para `v1.6.54`, que também corrige a regressão CVE-2026-22695 presente em 1.6.51–1.6.53.
- Possível quebra: baixa; deve ser validada compilando todas as arquiteturas e abrindo assets/mods PNG.

### 4. Alto — textos dinâmicos usados como string de formato (corrigido)

- Locais: `Plandomizer.cpp` e `randomizer_check_tracker.cpp`.
- Risco: nomes, dicas ou dados contendo `%` eram enviados diretamente a `ImGui::Text`/`TextWrapped`; isso poderia provocar leitura indevida da pilha, vazamento de memória ou crash.
- Correção: todas as ocorrências encontradas passaram a usar formato constante `"%s"`.
- Possível quebra: nenhuma esperada; altera somente a forma segura de exibir texto.

### 5. Alto — Anchor transmite dados em texto claro (pendente; exige servidor/PC)

- Local: `soh/multiplayer/NetworkManager.cpp` e `MultiplayerManager.cpp`.
- Dados expostos na rede: hostname/IP de destino, Room ID, Team ID, nome escolhido, mensagens de chat, cena, posição, animação e aparência do jogador.
- Risco: alguém com capacidade de observar ou alterar a rede pode ler chats e identificadores escolhidos, modificar pacotes ou se passar pelo servidor.
- Correção recomendada: TLS 1.3 com validação normal de certificado, autenticação de sessão, proteção contra replay e autorização de sala. O servidor deve limitar tamanho/taxa e nunca confiar em `clientId` fornecido pelo cliente.
- Possível quebra: sim. Requer nova versão do protocolo e atualização conjunta do Anchor de PC, servidor e Android. Não foi aplicada unilateralmente para preservar compatibilidade.
- Mitigação atual: não reutilizar senhas reais como Room ID/Team ID/nome; evitar redes Wi‑Fi não confiáveis; não enviar dados pessoais pelo chat.

### 6. Médio — backup de saves/configurações (corrigido)

- Local: `AndroidManifest.xml:68-70` e `res/xml/data_extraction_rules.xml`.
- Risco: backup Android poderia copiar nome, configuração de servidor/sala, saves e preferências para backup em nuvem ou transferência de aparelho.
- Correção: `allowBackup=false`, `fullBackupContent=false` e regras explícitas excluindo todos os domínios.

### 7. Médio — negação de serviço por filas/pacotes de rede (corrigido)

- Locais: `NetworkManager.cpp:170-330` e `MultiplayerManager.cpp:53,130`.
- Risco: servidor malicioso poderia enviar JSON sem delimitador, rajadas ilimitadas, IDs ilimitados ou números não finitos e consumir memória/travar renderização.
- Correções: JSON limitado a 64 KiB, buffer total a 512 KiB, fila a 64 mensagens, fila de envio a 64 KiB, máximo de 64 jogadores remotos, validação de números finitos e limites de texto. `MSG_NOSIGNAL` evita encerramento por socket fechado.
- Compatibilidade Anchor: a fila descarta o pacote mais antigo sob rajada, sem desconectar. O limite anterior de 16 KiB foi ampliado porque causava ciclo de conexão/desconexão com pacotes Anchor válidos.

### 8. Médio — logs contendo dados de usuário/rede (corrigido)

- Locais: multiplayer, Sail, CrowdControl, `Network.cpp` e `MainActivity.java`.
- Risco: logs registravam host/porta, Room ID, nomes, IDs, posições, payloads JSON e caminhos de arquivos.
- Correção: valores foram removidos dos logs; eventos genéricos permanecem para diagnóstico.

### 9. Médio — dependências antigas e cadeia de build (revisão manual)

- AndroidX Core está em `1.7.0` e ConstraintLayout em `2.1.4`; versões estáveis mais recentes existem, mas a atualização exige elevar `compileSdk` e testar Android 7+.
- Boost está fixado em 1.81.0. SDL2 2.32.8, nlohmann-json 3.11.3, tinyxml2 10.0.0, spdlog 1.14.1 e libzip 1.10.1 devem ser revisados periodicamente.
- Várias dependências CMake são obtidas por tag Git, não por hash imutável. Recomendação: fixar commits completos e manter hashes de arquivos baixados para reduzir risco de cadeia de suprimentos.
- Não foi afirmada a existência de CVE onde não houve confirmação.

### 10. Baixo — ofuscação/R8 desativados (pendente)

- Local: `Android/app/build.gradle:37` (`minifyEnabled false`).
- Impacto: facilita engenharia reversa, mas ofuscação não substitui controles de segurança e não protege segredos embutidos.
- Recomendação: testar `minifyEnabled true` e `shrinkResources true` somente na release, com regras para SDL/JNI. Pode quebrar classes chamadas via JNI/reflexão; revisão manual necessária.

## Superfície Android verificada

- `MainActivity` é exportada apenas porque é a Activity launcher; não foi encontrado processamento de extras sensíveis.
- Nenhum Service, BroadcastReceiver ou ContentProvider próprio exportado.
- Nenhum WebView/JavaScript.
- Nenhum FileProvider.
- Nenhuma permissão de câmera, microfone, localização, contatos, telefone ou Bluetooth ativa.
- SharedPreferences usa `MODE_PRIVATE`; não contém credenciais.
- Nenhum segredo hardcoded foi encontrado no código analisado.
- O APK contém assets/OTR do aplicativo, mas não continha saves nem dados pessoais de usuários no momento da inspeção.

## APK inspecionado

Arquivo: `Android/app/build/outputs/apk/debug/SOH-Mobile-Anchor.apk`.

- Pacote: `com.dishii.soh`.
- ABI: `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`.
- Permissões: somente `INTERNET` e `VIBRATE`.
- Backup: desativado no manifesto mesclado.
- Assinatura: APK Signature Scheme v2, certificado `Android Debug`.
- Estado: adequado para teste local após recompilar; inadequado para distribuição pública por ser debug.

As últimas correções de reconexão Anchor, posição do chat, libpng e strings de formato foram feitas depois da última compilação inspecionada. Recompile antes de testar/distribuir.

## Checklist antes de publicar

- [ ] Criar uma chave de assinatura privada de release e guardá-la fora do repositório.
- [ ] Gerar `assembleRelease`, assinar e verificar com `apksigner verify --verbose --print-certs`.
- [ ] Confirmar `debuggable=false` no APK de release.
- [ ] Implementar TLS/autenticação no protocolo Anchor em conjunto com PC/servidor.
- [ ] Testar conexão, chat, nomes/cores, adulto/criança e troca de cena em PC + Android.
- [ ] Testar migração de saves da antiga `/SOH` em Android 7, 10, 11 e versões atuais.
- [ ] Apagar a antiga `/SOH` após confirmar a migração.
- [ ] Testar libpng 1.6.54 com assets e mods.
- [ ] Atualizar compileSdk/targetSdk e AndroidX em uma etapa separada.
- [ ] Ativar R8 apenas depois de testes JNI completos.
- [ ] Repetir análise de dependências/CVEs imediatamente antes da publicação.
- [ ] Publicar política de privacidade simples explicando nome, chat, IP e telemetria de jogo enviada ao Anchor.

## Referências primárias

- Android — all-files access: https://developer.android.com/training/data-storage/manage-all-files
- Android — app-specific storage: https://developer.android.com/training/data-storage/app-specific
- Android — cleartext communications: https://developer.android.com/privacy-and-security/risks/cleartext-communications
- Android — app signing: https://developer.android.com/studio/publish/app-signing
- libpng CVE-2025-64720: https://github.com/pnggroup/libpng/security/advisories/GHSA-hfc7-ph9c-wcww
- libpng CVE-2026-22695: https://github.com/pnggroup/libpng/security/advisories/GHSA-mmq5-27w3-rxpp
