package com.kail.location.ads

import android.view.LayoutInflater
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.google.android.gms.ads.nativead.NativeAd
import com.google.android.gms.ads.nativead.NativeAdView
import com.kail.location.R

@Composable
fun NativeAdCard(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    var nativeAd by remember { mutableStateOf<NativeAd?>(null) }

    LaunchedEffect(Unit) {
        NativeAdLoader.loadAd(context) { ad ->
            nativeAd = ad
        }
    }

    nativeAd?.let { ad ->
        Card(
            modifier = modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp),
            shape = RoundedCornerShape(12.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
        ) {
            AndroidView(
                factory = { ctx ->
                    val adView = LayoutInflater.from(ctx)
                        .inflate(R.layout.native_ad_layout, null) as NativeAdView

                    val headline = adView.findViewById<android.widget.TextView>(R.id.ad_headline)
                    headline.text = ad.headline ?: ""
                    adView.headlineView = headline

                    val body = adView.findViewById<android.widget.TextView>(R.id.ad_body)
                    body.text = ad.body ?: ""
                    adView.bodyView = body

                    val cta = adView.findViewById<android.widget.Button>(R.id.ad_call_to_action)
                    cta.text = ad.callToAction ?: "查看"
                    adView.callToActionView = cta

                    val icon = adView.findViewById<android.widget.ImageView>(R.id.ad_app_icon)
                    ad.icon?.drawable?.let { icon.setImageDrawable(it) }
                    adView.iconView = icon

                    adView.setNativeAd(ad)
                    adView
                },
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}
