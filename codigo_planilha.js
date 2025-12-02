// Define o nome da aba que receberá os dados
var SHEET_NAME = "Dados_ESP"; 

function doGet(e) {
  try {
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(SHEET_NAME);

    // Pega os parâmetros da URL (agora incluindo id e latitude)
    var id = e.parameter.id; // ToDo: alterar para valor real
    var latitude = e.parameter.lat; // ToDo: alterar para valor real
    var longitude = e.parameter.lon;
    var temperatura = e.parameter.temperature;
    var umidade = e.parameter.humidity;
    var timestamp = new Date; // Gera o obj data no momento que ele recebe a mensagem do esp

    // Adiciona uma nova linha com os dados
    sheet.appendRow([id, temperatura, umidade, latitude, longitude, timestamp]);

    // Retorna uma resposta de sucesso
    return ContentService.createTextOutput("Dados recebidos com sucesso.").setMimeType(ContentService.MimeType.TEXT);
  } catch (error) {
    // Retorna uma resposta de erro
    return ContentService.createTextOutput("Erro: " + error.message).setMimeType(ContentService.MimeType.TEXT);
  }
}
