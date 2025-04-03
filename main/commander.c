#include "commander.h"
#include "write.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

#define TAG "COMMANDER"
#define MAX_COMMANDS 9 // só aceita de '1' a '9'

static char command_list[MAX_COMMANDS]; // agora só os dígitos
static int command_count = 0;

void commander_init(void)
{
  memset(command_list, 0, sizeof(command_list));
  command_count = 0;
  write_default("TODOS");
}

static void sort_command_list()
{
  for (int i = 0; i < command_count - 1; i++)
  {
    for (int j = i + 1; j < command_count; j++)
    {
      if (command_list[i] > command_list[j])
      {
        char temp = command_list[i];
        command_list[i] = command_list[j];
        command_list[j] = temp;
      }
    }
  }
}

static void remove_command(char key)
{
  for (int i = 0; i < command_count; i++)
  {
    if (command_list[i] == key)
    {
      for (int j = i; j < command_count - 1; j++)
      {
        command_list[j] = command_list[j + 1];
      }
      command_count--;
      break;
    }
  }
}

void commander_process_key(char key)
{
  if (key == 'D')
  {
    ESP_LOGI(TAG, "Comando apagado.");
    memset(command_list, 0, sizeof(command_list));
    command_count = 0;
    write_default("TODOS");
    return;
  }

  // Só aceita números de '1' a '9'
  if (key < '1' || key > '9')
  {
    ESP_LOGW(TAG, "Ignorado: caractere inválido '%c'", key);
    return;
  }

  // Se já existe, remove e finaliza
  for (int i = 0; i < command_count; i++)
  {
    if (command_list[i] == key)
    {
      ESP_LOGI(TAG, "Removendo comando duplicado: %c", key);
      remove_command(key);
      sort_command_list(); // manter ordenado após remoção
      goto update_display;
    }
  }

  // Se não existia, adiciona
  if (command_count < MAX_COMMANDS)
  {
    command_list[command_count++] = key;
    sort_command_list();
  }

update_display:

  // Monta string de exibição
  if (command_count == 0)
  {
    write_default("TODOS");
    return;
  }

  char display_string[128];
  strcpy(display_string, "AR  ");

  for (int i = 0; i < command_count; i++)
  {
    char part[4];
    snprintf(part, sizeof(part), "%c", command_list[i]);
    strcat(display_string, part);

    if (i < command_count - 1)
    {
      strcat(display_string, " - ");
    }
  }

  write_default(display_string);
}
