output "public_ip_address" {
  value = azurerm_public_ip.main.ip_address
}

output "ssh_command" {
  value = "ssh -i ~/.ssh/webserver_azure ${var.admin_username}@${azurerm_public_ip.main.ip_address}"
}

output "deploy_path" {
  value = var.deploy_path
}
