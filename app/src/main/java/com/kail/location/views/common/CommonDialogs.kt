package com.kail.location.views.common

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.kail.location.R
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.ui.unit.dp
import com.kail.location.models.UpdateInfo

/**
 * Composable function to display an update dialog.
 *
 * @param info The update information to display.
 * @param onDismiss Callback when the dialog is dismissed.
 * @param onConfirm Callback when the confirm button (Download) is clicked.
 */
@Composable
fun UpdateDialog(
    info: UpdateInfo,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.update_title)) },
        text = {
             Text(info.content)
        },
        confirmButton = {
            TextButton(onClick = onConfirm) {
                Text(stringResource(R.string.update_download))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.common_cancel))
            }
        }
    )
}

/**
 * 更新下载对话框，支持显示下载进度。
 *
 * @param info 更新信息。
 * @param downloading 是否正在下载。
 * @param progress 下载进度（0-100）。
 * @param onDismiss 关闭回调。
 * @param onStartDownload 开始下载回调。
 */
@Composable
fun UpdateDownloadDialog(
    info: UpdateInfo,
    downloading: Boolean,
    progress: Int,
    onDismiss: () -> Unit,
    onStartDownload: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.update_title)) },
        text = {
            Column {
                Text(info.content)
                Spacer(modifier = Modifier.height(12.dp))
                if (downloading) {
                    LinearProgressIndicator(
                        progress = progress / 100f,
                        modifier = Modifier.fillMaxWidth(),
                        color = MaterialTheme.colorScheme.primary
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(stringResource(R.string.download_progress, progress))
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                if (!downloading) onStartDownload()
            }) {
                Text(if (downloading) stringResource(R.string.update_downloading) else stringResource(R.string.update_download))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.common_cancel))
            }
        }
    )
}
